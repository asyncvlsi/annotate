/*************************************************************************
 *
 *  Copyright (c) 2022 Rajit Manohar
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include <common/misc.h>
#include "spef.h"

#define MAP_GET_PTR(x)  ((ActId *)(((unsigned long)(x))&~3UL))
#define MAP_MK_ABS(x) ((ActId *) (((unsigned long)(x))|1))
#define MAP_MK_REF(x) ((ActId *) (((unsigned long)(x))|2))
#define MAP_IS_REF(x) (((unsigned long)x) & 2)

Spef::Spef()
{
  _l = NULL;
#define TOKEN(a,b)  a = -1;  
#include "spef.def"

  _spef_version = NULL;
  _design_name = NULL;
  _date = NULL;
  _vendor = NULL;
  _program = NULL;
  _version = NULL;

  _tok_hier_delim = -1;
  _tok_pin_delim = -1;
  _tok_prefix_bus_delim = -1;
  _tok_suffix_bus_delim = -1;
  _nH = NULL;

  A_INIT (_power_nets);
  A_INIT (_gnd_nets);
  A_INIT (_ports);
  A_INIT (_phyports);
  A_INIT (_defines);
}

static void _free_id (ActId *id)
{
  if (!id) return;
  if (!MAP_IS_REF (id)) {
    delete MAP_GET_PTR (id);
  }
}

Spef::~Spef()
{
  if (_l) {
    lex_free (_l);
    _l = NULL;
  }

  if (_spef_version) {
    FREE (_spef_version);
  }
  if (_design_name) {
    FREE (_design_name);
  }
  if (_date) {
    FREE (_date);
  }
  if (_vendor) {
    FREE (_vendor);
  }
  if (_program) {
    FREE (_program);
  }
  if (_version) {
    FREE (_version);
  }

  if (_nH) {
    ihash_bucket_t *b;
    ihash_iter_t it;
    
    ihash_iter_init (_nH, &it);
    while ((b = ihash_iter_next (_nH, &it))) {
      if (b->v) {
	ActId *id = MAP_GET_PTR (b->v);
	delete id;
      }
    }
    ihash_free (_nH);
    _nH = NULL;
  }

  for (int i=0; i < A_LEN (_power_nets); i++) {
    _free_id (_power_nets[i]);
  }
  for (int i=0; i < A_LEN (_gnd_nets); i++) {
    _free_id (_gnd_nets[i]);
  }
  A_FREE (_power_nets);
  A_FREE (_gnd_nets);

  for (int i=0; i < A_LEN (_ports); i++) {
    if (_ports[i].a) {
      FREE (_ports[i].a);
    }
    if (_ports[i].inst) {
      _free_id (_ports[i].inst);
    }
    _free_id (_ports[i].port);
  }
  for (int i=0; i < A_LEN (_phyports); i++) {
    if (_phyports[i].a) {
      FREE (_phyports[i].a);
    }
    _free_id (_phyports[i].inst);
    _free_id (_phyports[i].port);
  }
  A_FREE (_phyports);

  for (int i=0; i < A_LEN (_defines); i++) {
    _free_id (_defines[i].inst);
    if (_defines[i].qstring) {
      FREE (_defines[i].qstring);
    }
  }
  A_FREE (_defines);
}

bool Spef::Read (FILE *fp)
{
  _l = lex_file (fp);

  /*-- add tokens --*/
#define TOKEN(a,b) a = lex_addtoken (_l, b);
#include "spef.def"

  lex_getsym (_l);

  if (!_read_header ()) {
    return false;
  }

  if (!_read_units ()) {
    return false;
  }
  
  if (!_read_name_map ()) {
    return false;
  }
  
  if (!_read_power_def ()) {
    return false;
  }

  if (!_read_external_def ()) {
    return false;
  }

  if (!_read_define_def ()) {
    return false;
  }

  if (!_read_variation_def ()) {
    return false;
  }
  
  if (!_read_internal_def ()) {
    return false;
  }

  lex_free (_l);
  _l = NULL;
  return true;
}

char *Spef::_prevString ()
{
  char *tmp = Strdup (lex_prev (_l) + 1);
  tmp[strlen(tmp)-1] = '\0';
  return tmp;
}

bool Spef::_read_header ()
{
#define GET_STR(a,b,msg)			\
  do {						\
    if (!lex_have (_l, a)) {			\
      warning ("SPEF parsing error: missing " msg);	\
      return false;				\
    }						\
    if (lex_have (_l, l_string)) {		\
      b = _prevString ();			\
    }						\
    else {					\
      warning ("SPEF parsing error: invalid " msg);	\
      return false;				\
    }						\
  } while (0)

  GET_STR(_star_spef, _spef_version, "*SPEF in header");
  GET_STR(_star_design, _design_name, "*DESIGN in header");
  GET_STR(_star_date, _date, "*DATE in header");
  GET_STR(_star_vendor, _vendor, "*VENDOR in header");
  GET_STR(_star_program, _program, "*PROGRAM in header");
  GET_STR(_star_version, _version, "*VERSION in header");

  if (!lex_have (_l, _star_design_flow)) {
    warning ("SPEF parsing error: missing *DESIGN_FLOW in header");
    return false;
  }
  if (lex_sym (_l) != l_string) {
    warning ("SPEF parsing error: invalid *DESIGN_FLOW in header");
  }
  while (lex_have (_l, l_string)) {
    /* grab string (page 592: check strings here) */
  }
    
  if (!lex_have (_l, _star_divider)) {
    warning ("SPEF parsing error: missing *DIVIDER in header");
  }
  /* ., /, :, or | */
  if (strcmp (lex_tokenstring (_l), ".") == 0 ||
      strcmp (lex_tokenstring (_l), "/") == 0 ||
      strcmp (lex_tokenstring (_l), ":") == 0 ||
      strcmp (lex_tokenstring (_l), "|") == 0) {
    _tok_hier_delim = lex_addtoken (_l, lex_tokenstring (_l));
  }
  else {
    warning ("SPEF parsing error: *DIVIDER must be one of . / : |");
    return false;
  }

  lex_getsym (_l);
  
  if (!lex_have (_l, _star_delimiter)) {
    warning ("SPEF parsing error: missing *DELIMITER in header");
    return false;
  }
  /* ., /, :, or | */
  if (strcmp (lex_tokenstring (_l), ".") == 0 ||
      strcmp (lex_tokenstring (_l), "/") == 0 ||
      strcmp (lex_tokenstring (_l), ":") == 0 ||
      strcmp (lex_tokenstring (_l), "|") == 0) {
    _tok_pin_delim = lex_addtoken (_l, lex_tokenstring (_l));
  }
  else {
    warning ("SPEF parsing error: *DELIMITER must be one of . / : |");
    return false;
  }

  lex_getsym (_l);
  
  if (!lex_have (_l, _star_bus_delimiter)) {
    warning ("SPEF parsing error: missing *BUS_DELIMITER in header");
    return false;
  }
  if (strcmp (lex_tokenstring (_l), "[") == 0 ||
      strcmp (lex_tokenstring (_l), "{") == 0 ||
      strcmp (lex_tokenstring (_l), "(") == 0 ||
      strcmp (lex_tokenstring (_l), "<") == 0 ||
      strcmp (lex_tokenstring (_l), ":") == 0 ||
      strcmp (lex_tokenstring (_l), ".") == 0) {
    _tok_prefix_bus_delim = lex_addtoken (_l, lex_tokenstring (_l));
  }
  else {
    warning ("SPEF parsing error: *BUS_DELIMITER must be one of [ { ( < : .");
    return false;
  }
  lex_getsym (_l);
  if (strcmp (lex_tokenstring (_l), "]") == 0 ||
      strcmp (lex_tokenstring (_l), "}") == 0 ||
      strcmp (lex_tokenstring (_l), ")") == 0 ||
      strcmp (lex_tokenstring (_l), ">") == 0) {
    _tok_suffix_bus_delim = lex_addtoken (_l, lex_tokenstring (_l));
    lex_getsym (_l);
  }
  return true;
}


bool Spef::_read_units ()
{
  double val;
  if (!lex_have (_l, _star_t_unit)) {
    warning ("SPEF parsing error: *T_UNIT missing");
    return false;
  }
  if (lex_have (_l, l_integer)) {
    val = lex_integer (_l);
  }
  else if (lex_have (_l, l_real)) {
    val = lex_real (_l);
  }
  else {
    warning ("SPEF parsing error: *T_UNIT expected number");
    return false;
  }
  if (val < 0) {
    warning ("SPEF parsing error: *T_UNIT expected positive number");
    return false;
  }
  if (lex_have_keyw (_l, "NS")) {
    val = val*1e-9;
  }
  else if (lex_have_keyw (_l, "PS")) {
    val = val*1e-12;
  }
  else {
    warning ("SPEF parsing error: *T_UNIT expected NS or PS");
    return false;
  }
  _time_unit = val;

  if (!lex_have (_l, _star_c_unit)) {
    warning ("SPEF parsing error: *C_UNIT missing");
    return false;
  }
  if (lex_have (_l, l_integer)) {
    val = lex_integer (_l);
  }
  else if (lex_have (_l, l_real)) {
    val = lex_real (_l);
  }
  else {
    warning ("SPEF parsing error: *C_UNIT expected number");
    return false;
  }
  if (val < 0) {
    warning ("SPEF parsing error: *C_UNIT expected positive number");
    return false;
  }
  if (lex_have_keyw (_l, "PF")) {
    val = val*1e-12;
  }
  else if (lex_have_keyw (_l, "FF")) {
    val = val*1e-15;
  }
  else {
    warning ("SPEF parsing error: *C_UNIT expected PF or FF");
    return false;
  }
  _c_unit = val;


  if (!lex_have (_l, _star_r_unit)) {
    warning ("SPEF parsing error: *R_UNIT missing");
    return false;
  }
  if (lex_have (_l, l_integer)) {
    val = lex_integer (_l);
  }
  else if (lex_have (_l, l_real)) {
    val = lex_real (_l);
  }
  else {
    warning ("SPEF parsing error: *R_UNIT expected number");
    return false;
  }
  if (val < 0) {
    warning ("SPEF parsing error: *R_UNIT expected positive number");
    return false;
  }
  if (lex_have_keyw (_l, "OHM")) {
    /* nothing */
  }
  else if (lex_have_keyw (_l, "KOHM")) {
    val = val*1e3;
  }
  else {
    warning ("SPEF parsing error: *R_UNIT expected OHM or KOHM");
    return false;
  }
  _r_unit = val;

  if (!lex_have (_l, _star_l_unit)) {
    warning ("SPEF parsing error: *L_UNIT missing");
    return false;
  }
  if (lex_have (_l, l_integer)) {
    val = lex_integer (_l);
  }
  else if (lex_have (_l, l_real)) {
    val = lex_real (_l);
  }
  else {
    warning ("SPEF parsing error: *L_UNIT expected number");
    return false;
  }
  if (val < 0) {
    warning ("SPEF parsing error: *L_UNIT expected positive number");
    return false;
  }
  if (lex_have_keyw (_l, "MH")) {
    val = val*1e-3;
  }
  else if (lex_have_keyw (_l, "UH")) {
    val = val*1e-6;
  }
  else if (lex_have_keyw (_l, "HENRY")) {
    /* nothing */
  }
  else {
    warning ("SPEF parsing error: *L_UNIT expected HENRY or MH or UH");
    return false;
  }
  _l_unit = val;

  return true;
}


bool Spef::_read_name_map ()
{
  ihash_bucket_t *b;
  
  if (!lex_have (_l, _star_name_map)) {
    /* no name map */
    return true;
  }

  _nH = ihash_new (16);

  while (strcmp (lex_tokenstring (_l), "*") == 0) {
    lex_getsym (_l);
    if (strcmp (lex_whitespace (_l), "") != 0) {
      warning ("SPEF parsing: space after *, ignoring");
    }
    if (lex_have (_l, l_integer)) {
      /* we're good */
      b = ihash_lookup (_nH, lex_integer (_l));
      if (b) {
	warning ("SPEF *NAME_MAP: duplicate integer %d; using latest map",
		 lex_integer (_l));
      }
      else {
	b = ihash_add (_nH, lex_integer (_l));
	b->v = NULL;
      }
    }
    else {
      warning ("SPEF parsing: missing integer after * in name map");
      return false;
    }

    b->v = _getTokPhysicalRef ();
    if (!b->v) {
      b->v = _getTokPath ();
    }
    if (!b->v) {
      warning ("SPEF parsing: *%ld error parsing name", b->key);
      return false;
    }
  }
  return true;
}

bool Spef::_read_power_def ()
{
  ActId *tmp;
  if (lex_have (_l, _star_power_nets)) {
    /* power definition */
    while ((tmp = _getTokPath()) || (tmp = _getTokPhysicalRef())) {
      A_NEW (_power_nets, ActId *);
      A_NEXT (_power_nets) = tmp;
      A_INC (_power_nets);
    }
    if (A_LEN (_power_nets) == 0) {
      warning ("SPEF parsing: *POWER_NETS error");
      return false;
    }
  }
  if (lex_have (_l, _star_ground_nets)) {
    /* ground def */
    while ((tmp = _getTokPath()) || (tmp = _getTokPhysicalRef())) {
      A_NEW (_gnd_nets, ActId *);
      A_NEXT (_gnd_nets) = tmp;
      A_INC (_gnd_nets);
    }
    if (A_LEN (_gnd_nets) == 0) {
      warning ("SPEF parsing: *POWER_NETS error");
      return false;
    }
  }

  return true;
}

bool Spef::_getPortName (bool isphy, ActId **inst_name, ActId **port)
{
  ActId *tmp, *tmp2;

  lex_push_position (_l);

  if ((tmp = _getIndex()) || (!isphy && (tmp = _getTokPath())) ||
      (isphy && (tmp = _getTokName()))) {
    if (_tok_pin_delim != -1 && lex_have (_l, _tok_pin_delim)) {
      tmp2 = _getIndex();
      if (!tmp2) {
	tmp2 = _getTokPath ();
      }
      if (!tmp2) {
	lex_set_position (_l);
	lex_pop_position (_l);
	warning ("SPEF parsing: port name error");
	return false;
      }

      *inst_name = tmp;
      *port = tmp2;

      return true;
    }
    else {
      tmp2 = MAP_GET_PTR (tmp);
      if (tmp2->Rest()) {
	lex_set_position (_l);
	lex_pop_position (_l);
	warning ("SPEF parsing: port name error");
	return false;
      }
      *inst_name = NULL;
      *port = tmp;
      return true;
    }
  }
  return false;
}

static int lex_get_dir (LEX_T *l)
{
  if (lex_have_keyw (l, "I")) {
    return 0;
  }
  else if (lex_have_keyw (l, "O")) {
    return 1;
  }
  else if (lex_have_keyw (l, "B")) {
    return 2;
  }
  else {
    return -1;
  }
}

bool Spef::_read_external_def ()
{
  int  count = 0;
  int tok;
  int found = 0;

  tok = _star_ports;

  while (count < 2) {
    count++;
    if (lex_have (_l, tok)) {
      ActId *inst, *port;
      bool once = false;
      bool typ;

      found = 1;
      if (tok == _star_ports) {
	typ = false;
      }
      else {
	typ = true;
      }

      while (_getPortName (typ, &inst, &port)) {
	int dir;
	once = true;

	dir = lex_get_dir (_l);
	if (dir == -1) {
	  warning ("SPEF parsing: %s direction error", lex_tokenname (_l, tok));
	  return false;
	}
	spef_attributes *a = _getAttributes ();

	/*-- save this away --*/
	if (typ == false) {
	  A_NEW (_ports, spef_ports);
	  A_NEXT (_ports).a = a;
	  A_NEXT (_ports).inst = inst;
	  A_NEXT (_ports).port = port;
	  A_NEXT (_ports).dir = dir;
	  A_INC (_ports);
	}
	else {
	  A_NEW (_phyports, spef_ports);
	  A_NEXT (_phyports).a = a;
	  A_NEXT (_phyports).inst = inst;
	  A_NEXT (_phyports).port = port;
	  A_NEXT (_phyports).dir = dir;
	  A_INC (_phyports);
	}
      }
      if (!once) {
	warning ("SPEF parsing: %s error", lex_tokenname (_l, tok));
	return false;
      }
    }
    tok = _star_physical_ports;
  }
  return true;
}

bool Spef::_read_define_def ()
{
  while (1) {
    if (lex_have (_l, _star_define)) {
      int idx;
      ActId *tmp;
      idx = A_LEN (_defines);
      while ((tmp = _getIndex()) || (tmp = _getTokPath())) {
	A_NEW (_defines, spef_defines);
	A_NEXT (_defines).phys = 0;
	A_NEXT (_defines).inst = tmp;
	A_NEXT (_defines).qstring = NULL;
	A_INC (_defines);
      }
      if (idx == A_LEN (_defines)) {
	warning ("SPEF parsing: *DEFINE error\n%s", lex_errstring (_l));
	return false;
      }
      char *str;
      if (lex_have (_l, l_string)) {
	str = _prevString();
      }
      else {
	warning ("SPEF parsing: *DEFINE error\n%s", lex_errstring (_l));
	return false;
      }
      while (idx < A_LEN (_defines)) {
	if (str) {
	  _defines[idx].qstring = str;
	  str = NULL;
	}
	else {
	  _defines[idx].qstring = Strdup (_defines[idx-1].qstring);
	}
	idx++;
      }
    }
    else if (lex_have (_l, _star_pdefine)) {
      ActId *tmp;
      if ((tmp = _getIndex()) || (tmp = _getTokPath())) {
	A_NEW (_defines, spef_defines);
	A_NEXT (_defines).phys = 1;
	A_NEXT (_defines).inst = tmp;
	A_NEXT (_defines).qstring = NULL;
      }
      else {
	warning ("SPEF parsing: *PDEFINE error\n%s", lex_errstring (_l));
	return false;
      }
      if (lex_have (_l, l_string)) {
	A_NEXT (_defines).qstring = _prevString();
	A_INC (_defines);
      }
      else {
	A_INC (_defines);
	warning ("SPEF parsing: *PDEFINE error\n%s", lex_errstring (_l));
	return false;
      }
    }
    else {
      return true;
    }
  }
}

bool Spef::_read_variation_def ()
{
  if (!lex_have (_l, _star_variation_parameters)) {
    return true;
  }
  fatal_error ("Need to parse *VARIATION_PARAMETERS!");
  return true;
}

bool Spef::_read_internal_def ()
{
  bool found = false;
  while (lex_sym (_l) == _star_d_net || lex_sym (_l) == _star_r_net ||
	 lex_sym (_l) == _star_d_pnet || lex_sym (_l) == _star_r_pnet) {
    found = true;

    if (lex_have (_l, _star_d_net)) {
      ActId *net_name;
      if (!((net_name = _getIndex()) || (net_name = _getTokPath()))) {
	warning ("SPEF parsing: *D_NET error\n%s", lex_errstring (_l));
	return false;
      }
      spef_triplet tot_cap;
      if (!_getParasitics (&tot_cap)) {
	warning ("SPEF parsing: *D_NET error\n%s", lex_errstring (_l));
	return false;
      }

      /* routing conf = routing confidence value
	   10 = statistical wire load model
	   20 = physical wire load model
	   30 = physical partitions with locations, no cell placement
	   40 = estimated cell placement with steiner tree based route
	   50 = estimated cell placement with global route
	   60 = final cell placement with steiner route
	   70 = final cell placement with global route
	   80 = final cell placement, final route, 2d extraction
	   90 = final cell placement, final route, 2.5d extraction
	   100 = final cell placement, final route, 3d extraction
       */
      int routing_conf = -1;
      if (lex_have (_l, _star_v)) {
	if (lex_have (_l, l_integer)) {
	  routing_conf = lex_integer (_l);
	}
	else {
	  warning ("SPEF parsing: *D_NET error\n%s", lex_errstring (_l));
	  return false;
	}
      }
      
      if (lex_have (_l, _star_conn)) {
	/* optional connection section */
	/* *P or *I parts */
	bool found = false;
	while (lex_sym (_l) == _star_p || lex_sym (_l) == _star_i) {
	  ActId *inst, *pin;
	  int dir;
	  found = true;
	  if (lex_have (_l, _star_p)) {
	    /* port name or pport name */
	    if (!_getPortName (false, &inst, &pin) &&
		!_getPortName (true, &inst, &pin)) {
	      warning ("SPEF parsing: *P missing port\n%s",
		       lex_errstring (_l));
	      return false;
	    }
	  }
	  else if (lex_have (_l, _star_i)) {
	    /* pin name or pnode ref */
	    ActId *inst_name, *pin;
	    if ((inst_name = _getIndex()) || (inst_name = _getTokPath())) {
	      /* pin delim */
	      if (!lex_have (_l, _tok_pin_delim)) {
		warning ("SPEF parsing: *I pin error\n%s",
			 lex_errstring (_l));
		return false;
	      }
	      pin = _getIndex();
	      if (!pin) {
		pin = _getTokPath();
		if (!pin) {
		  warning ("SPEF parsing: *I pin error\n%s",
			   lex_errstring (_l));
		  return false;
		}
	      }
	      if (MAP_GET_PTR(pin)->Rest()) {
		warning ("SPEF parsing: *I pin error\n%s",
			 lex_errstring (_l));
		return false;
	      }
	    }
	    else if ((inst = _getTokPhysicalRef ())) {
	      if (!lex_have (_l, _tok_pin_delim)) {
		warning ("SPEF parsing: *I pin error\n%s",
			 lex_errstring (_l));
		return false;
	      }
	      pin = _getIndex();
	      if (!pin) {
		pin = _getTokName();
		if (!pin) {
		  warning ("SPEF parsing: *I pin error\n%s",
			   lex_errstring (_l));
		  return false;
		}
	      }
	    }
	    else {
	      warning ("SPEF parsing: *I pin error\n%s", lex_errstring (_l));
	      return false;
	    }
	  }
	  else {
	    Assert (0, "What?!");
	  }
	  dir = lex_get_dir (_l);
	  if (dir == -1) {
	    warning ("SPEF parsing: *CONN direction error\n%s",
		     lex_errstring (_l));
	    return false;
	  }
	  spef_attributes *a = _getAttributes ();
	}
	if (!found) {
	  warning ("SPEF parsing: *CONN missing a conn_def\n%s",
		   lex_errstring (_l));
	  return false;
	}
	

	/* *N parts */
	
	
      }
      if (lex_have (_l, _star_cap)) {
	/* optional cap section */
      }
	
      if (lex_have (_l, _star_res)) {
	/* optional res section */
      }

      if (lex_have (_l, _star_induc)) {
	/* optional induc section */

      }

      if (!lex_have (_l, _star_end)) {
	warning ("SPEF parsing: *D_NET missing *END\n%s", lex_errstring (_l));
	return false;
      }
      
    }
    else if (lex_have (_l, _star_r_net)) {

    }
  }
  return found;
}


static int _valid_id_chars (char *s)
{
  while (*s) {
    if (((*s) >= 'a' && (*s) <= 'z') || ((*s) >= 'A' && (*s) <= 'Z') ||
	((*s) >= '0' && (*s) <= '9') ||	((*s) == '_')) {
      s++;
    }
    else if ((*s) == '\\' &&
	     (*(s+1) == '!' || *(s+1) == '#' || *(s+1) == '$' ||
	      *(s+1) == '%' || *(s+1) == '&' || *(s+1) == '\'' ||
	      *(s+1) == '(' || *(s+1) == ')' || *(s+1) == '*' ||
	      *(s+1) == '+' || *(s+1) == ',' || *(s+1) == '-' ||
	      *(s+1) == '+' || *(s+1) == ',' || *(s+1) == '-' ||
	      *(s+1) == '.' || *(s+1) == '/' || *(s+1) == ':' ||
	      *(s+1) == ';' || *(s+1) == '<' || *(s+1) == '=' ||
	      *(s+1) == '>' || *(s+1) == '?' || *(s+1) == '@' ||
	      *(s+1) == '[' || *(s+1) == '\\' || *(s+1) == ']' ||
	      *(s+1) == '^' || *(s+1) == '`' || *(s+1) == '{' ||
	      *(s+1) == '}' || *(s+1) == '~' || *(s+1) == '"')) {
      s += 2;
    }
    else {
      return 0;
    }
  }
  return 1;
}

/*
  Returns a SPEF identifier string from the token stream,
  or NULL if there isn't any
*/
char *Spef::_getTokId()
{
  char *curbuf;
  int bufsz, buflen;
  
  bufsz = 128;
  MALLOC (curbuf, char, bufsz);
  curbuf[0] = '\0';
  buflen = 0;

  /* next token is ID */
  while (_valid_id_chars (lex_tokenstring (_l))) {
    while (strlen (lex_tokenstring (_l)) + buflen + 1 > bufsz) {
      bufsz *= 2;
      REALLOC (curbuf, char, bufsz);
    }
    snprintf (curbuf + buflen, bufsz - buflen, "%s", lex_tokenstring (_l));
    buflen += strlen (curbuf + buflen);
    lex_getsym (_l);
    if (strcmp (lex_whitespace (_l), "") > 0) {
      break;
    }
  }
  if (buflen == 0) {
    FREE (curbuf);
    return NULL;
  }
  else {
    char *ret = Strdup (curbuf);
    FREE (curbuf);
    return ret;
  }
}

ActId *Spef::_getTokName()
{
  ActId *tmp;
  char *s;
  if (lex_have (_l, l_string)) {
    s = _prevString();
  }
  else {
    s = _getTokId ();
  }
  if (!s) {
    return NULL;
  }
  tmp = new ActId (s);
  FREE (s);
  return tmp;
}

ActId *Spef::_getTokPhysicalRef ()
{
  ActId *ret, *tmp;
  ret = NULL;

  lex_push_position (_l);

  while (lex_sym (_l) == l_string || _valid_id_chars (lex_tokenstring (_l))) {
    char *part;
    if (lex_have (_l, l_string)) {
      part = _prevString ();
    }
    else {
      part = _getTokId();
    }
    Assert (part, "Hmm");

    if (!ret) {
      ret = new ActId (part);
      tmp = ret;
    }
    else {
      tmp->Append (new ActId (part));
      tmp = tmp->Rest();
    }
    FREE (part);
    if (!lex_have (_l, _tok_hier_delim)) {
      break;
    }
    if (!(lex_sym (_l) == l_string || _valid_id_chars (lex_tokenstring (_l)))) {
      /*-- parse error --*/
      lex_set_position (_l);
      lex_pop_position (_l);
      if (ret) {
	delete ret;
      }
      return NULL;
    }
  }
  lex_pop_position (_l);
  return ret;
}


ActId *Spef::_getTokPath ()
{
  ActId *ret, *tmp;
  int isabs;
  
  ret = NULL;

  lex_push_position (_l);

  if (lex_have (_l, _tok_hier_delim)) {
    isabs = 1;
  }
  else {
    isabs = 0;
  }

  do {
    char *part = _getTokId ();
    if (!part) {
      lex_set_position (_l);
      lex_pop_position (_l);
      if (ret) {
	delete ret;
      }
      return NULL;
    }
    if (!ret) {
      ret = new ActId (part);
      tmp = ret;
    }
    else {
      tmp->Append (new ActId (part));
      tmp = tmp->Rest();
    }
    FREE (part);
  } while (lex_have (_l, _tok_hier_delim));

  if (lex_have (_l, _tok_prefix_bus_delim)) {
    if (!lex_have (_l, l_integer)) {
      delete ret;
      lex_set_position (_l);
      lex_pop_position (_l);
      return NULL;
    }
    Array *a = new Array (lex_integer (_l));
    tmp->setArray (a);
    if (_tok_suffix_bus_delim != -1 && lex_have (_l, _tok_suffix_bus_delim)) {
      /* nothing */
    }
  }
  lex_pop_position (_l);
  if (isabs) {
    return MAP_MK_ABS (ret);
  }
  else {
    return ret;
  }
}


ActId *Spef::_getIndex()
{
  if (strcmp (lex_tokenstring (_l), "*") == 0) {
    lex_push_position (_l);
    lex_getsym (_l);
    if (strcmp (lex_whitespace (_l), "") == 0 &&
	lex_sym (_l) == l_integer) {
      ihash_bucket_t *b;
      lex_getsym (_l);
      if (!_nH) {
	lex_set_position (_l);
	lex_pop_position (_l);
	return NULL;
      }
      b = ihash_lookup (_nH, lex_integer (_l));
      if (!b) {
	lex_set_position (_l);
	lex_pop_position (_l);
	return NULL;
      }
      lex_pop_position (_l);
      return MAP_MK_REF (b->v);
    }
    else {
      lex_set_position (_l);
      lex_pop_position (_l);
      return NULL;
    }
  }
  else {
    return NULL;
  }
}

static int lex_have_number (LEX_T *l, float *d)
{
  if (lex_have (l, l_integer)) {
    *d = lex_integer (l);
    return 1;
  }
  else if (lex_have (l, l_real)) {
    *d = lex_real (l);
    return 1;
  }
  return 0;
}

bool Spef::_getParasitics (spef_triplet *t)
{
  lex_push_position (_l);

  if (!lex_have_number (_l, &t->typ)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }
  if (!lex_have (_l, _tok_colon)) {
    t->best = t->typ;
    t->worst = t->typ;
    lex_pop_position (_l);
    return true;
  }
  t->best = t->typ;
  if (!lex_have_number (_l, &t->typ)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }
  if (!lex_have (_l, _tok_colon)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }
  if (!lex_have_number (_l, &t->worst)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }
  lex_pop_position (_l);
  return true;
}

spef_attributes *Spef::_getAttributes()
{
  spef_attributes *ret = NULL;

  while (lex_sym (_l) == _star_l || lex_sym (_l) == _star_c ||
	 lex_sym (_l) == _star_s || lex_sym (_l) == _star_d) {
    if (!ret) {
      NEW (ret, spef_attributes);
      ret->simple = 0;
      ret->coord = 0;
      ret->load = 0;
      ret->slew = 0;
      ret->slewth = 0;
      ret->drive = 0;
    }
    if (lex_have (_l, _star_l)) {
      if (ret->load) {
	warning ("SPEF parser: duplicate *L\n%s", lex_errstring (_l));
      }
      ret->load = 1;

      if (!_getParasitics (&ret->l)) {
	warning ("SPEF parser: parasitics error\n%s", lex_errstring (_l));
	FREE (ret);
	return NULL;
      }
    }
    else if (lex_have (_l, _star_c)) {
      if (ret->coord) {
	warning ("SPEF parser: duplicate *C\n%s", lex_errstring (_l));
      }
      ret->coord = 1;
      if (lex_have (_l, l_integer)) {
	ret->cx = lex_integer (_l);
      }
      else if (lex_have (_l, l_real)) {
	ret->cx = lex_real (_l);
      }
      else {
	warning ("SPEF parser: parasitics error\n%s", lex_errstring (_l));
	FREE (ret);
	return NULL;
      }
      if (lex_have (_l, l_integer)) {
	ret->cy = lex_integer (_l);
      }
      else if (lex_have (_l, l_real)) {
	ret->cy = lex_real (_l);
      }
      else {
	warning ("SPEF parser: parasitics error\n%s", lex_errstring (_l));
	FREE (ret);
	return NULL;
      }
    }
    else if (lex_have (_l, _star_s)) {
      if (ret->slew) {
	warning ("SPEF parser: duplicate *S\n%s", lex_errstring (_l));
      }
      ret->slew = 1;
      if (_getParasitics (&ret->s1) && _getParasitics (&ret->s2)) {
	if (_getParasitics (&ret->t1)) {
	  ret->slewth = 1;
	  if (!_getParasitics (&ret->t2)) {
	    warning ("SPEF parser: parasitics error\n%s", lex_errstring (_l));
	  }
	}
      }
      else {
	warning ("SPEF parser: parasitics error\n%s", lex_errstring (_l));
	FREE (ret);
	return NULL;
      }
    }
    else if (lex_have (_l, _star_d)) {
      if (ret->drive) {
	warning ("SPEF parser: duplicate *D");
      }
      ActId *tmp;
      tmp = _getIndex ();
      if (!tmp) {
	tmp = _getTokPath ();
      }
      if (!tmp) {
	warning ("SPEF parser: parasitics error\n%s", lex_errstring (_l));
	FREE (ret);
	return NULL;
      }
      ret->drive = 1;
      ret->cell = tmp;
    }
    else {
      fatal_error ("What?!");
    }
  }
  return ret;
}
