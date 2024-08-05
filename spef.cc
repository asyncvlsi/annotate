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
#include <common/ext.h>
#include "spef.h"

#define MAP_GET_PTR(x) SPEF_GET_PTR(x)
#define MAP_MK_ABS(x) ((ActId *) (((unsigned long)(x))|1))
#define MAP_MK_REF(x) ((ActId *) (((unsigned long)(x))|2))
#define MAP_IS_REF(x) (((unsigned long)x) & 2)
#define MAP_IS_ABS(x) SPEF_IS_ABS(x)

static void spef_warning (LEX_T *l, const char *s)
{
  warning ("SPEF parsing error: looking-at: `%s'\n\t%s\n%s",
	   lex_tokenstring (l), s, lex_errstring (l));
}

static int lex_have_number (LEX_T *l, float *d)
{
  if (lex_sym (l) == l_integer) {
    *d = lex_integer (l);
    lex_getsym (l);
    return 1;
  }
  else if (lex_sym (l) == l_real) {
    *d = lex_real (l);
    lex_getsym (l);
    return 1;
  }
  return 0;
}

static int lex_have_number (LEX_T *l, double *d)
{
  if (lex_sym (l) == l_integer) {
    *d = lex_integer (l);
    lex_getsym (l);
    return 1;
  }
  else if (lex_sym (l) == l_real) {
    *d = lex_real (l);
    lex_getsym (l);
    return 1;
  }
  return 0;
}

#define SKIP_SC_OPTIONAL			\
   do {						\
     if (lex_have (_l, _star_sc)) {		\
       while (lex_have (_l, l_integer)) {	\
	 float dummy;				\
	 if (!lex_have (_l, _tok_colon)) {	\
	   spef_warning (_l, "*SC error");	\
	   return false;			\
	 }					\
	 if (!lex_have_number (_l, &dummy)) {	\
	   spef_warning (_l, "*SC error");	\
	   return false;			\
	 }					\
       }					\
     }						\
   } while (0)


Spef::Spef(bool mangled_ids)
{
  _l = NULL;
#define TOKEN(a,b)  a = -1;  
#include "spef.def"

  _valid = 0;
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

  _divider = '?';
  _delimiter = '?';
  _bus_suffix_delim = '?';
  _bus_prefix_delim = '?';

  A_INIT (_power_nets);
  A_INIT (_gnd_nets);
  A_INIT (_ports);
  A_INIT (_phyports);
  A_INIT (_defines);
  _nets = NULL;

  if (mangled_ids) {
    _a = ActNamespace::Act();
    if (!_a) {
      _a = new Act();
    }
  }
  else {
    _a = NULL;
  }
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
  A_FREE (_ports);
  
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
    if (_defines[i].design_name) {
      FREE (_defines[i].design_name);
    }
  }
  A_FREE (_defines);
}

bool Spef::Read (const char *name)
{
  FILE *fp = fopen (name, "r");
  if (!fp) {
    fprintf (fp, "Spef::Read(): Could not open file `%s'", name);
    return false;
  }
  bool ret = Read (fp);
  fclose (fp);
  return ret;
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

  if (!lex_eof (_l)) {
    spef_warning (_l, "parsing ended without EOF?");
  }
  lex_free (_l);
  _l = NULL;
  _valid = 1;
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
      spef_warning (_l, "missing " msg);	\
      return false;				\
    }						\
    if (lex_have (_l, l_string)) {		\
      b = _prevString ();			\
    }						\
    else {					\
      spef_warning (_l, "invalid " msg);	\
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
    spef_warning (_l, "missing *DESIGN_FLOW in header");
    return false;
  }
  if (lex_sym (_l) != l_string) {
    spef_warning (_l, "invalid *DESIGN_FLOW in header");
  }
  while (lex_have (_l, l_string)) {
    /* grab string (page 592: check strings here) */
  }
    
  if (!lex_have (_l, _star_divider)) {
    spef_warning (_l, "missing *DIVIDER in header");
  }
  /* ., /, :, or | */
  if (strcmp (lex_tokenstring (_l), ".") == 0 ||
      strcmp (lex_tokenstring (_l), "/") == 0 ||
      strcmp (lex_tokenstring (_l), ":") == 0 ||
      strcmp (lex_tokenstring (_l), "|") == 0) {
    _tok_hier_delim = lex_addtoken (_l, lex_tokenstring (_l));
    _divider = lex_tokenstring(_l)[0];
  }
  else {
    spef_warning (_l, "*DIVIDER must be one of . / : |");
    return false;
  }

  lex_getsym (_l);
  
  if (!lex_have (_l, _star_delimiter)) {
    spef_warning (_l, "missing *DELIMITER in header");
    return false;
  }
  /* ., /, :, or | */
  if (strcmp (lex_tokenstring (_l), ".") == 0 ||
      strcmp (lex_tokenstring (_l), "/") == 0 ||
      strcmp (lex_tokenstring (_l), ":") == 0 ||
      strcmp (lex_tokenstring (_l), "|") == 0) {
    _tok_pin_delim = lex_addtoken (_l, lex_tokenstring (_l));
    _delimiter = lex_tokenstring(_l)[0];
  }
  else {
    spef_warning (_l, "*DELIMITER must be one of . / : |");
    return false;
  }

  lex_getsym (_l);
  
  if (!lex_have (_l, _star_bus_delimiter)) {
    spef_warning (_l, "missing *BUS_DELIMITER in header");
    return false;
  }
  if (strcmp (lex_tokenstring (_l), "[") == 0 ||
      strcmp (lex_tokenstring (_l), "{") == 0 ||
      strcmp (lex_tokenstring (_l), "(") == 0 ||
      strcmp (lex_tokenstring (_l), "<") == 0 ||
      strcmp (lex_tokenstring (_l), ":") == 0 ||
      strcmp (lex_tokenstring (_l), ".") == 0) {
    _tok_prefix_bus_delim = lex_addtoken (_l, lex_tokenstring (_l));
    _bus_prefix_delim = lex_tokenstring(_l)[0];
  }
  else {
    spef_warning (_l, "*BUS_DELIMITER must be one of [ { ( < : .");
    return false;
  }
  lex_getsym (_l);
  if (strcmp (lex_tokenstring (_l), "]") == 0 ||
      strcmp (lex_tokenstring (_l), "}") == 0 ||
      strcmp (lex_tokenstring (_l), ")") == 0 ||
      strcmp (lex_tokenstring (_l), ">") == 0) {
    _tok_suffix_bus_delim = lex_addtoken (_l, lex_tokenstring (_l));
    _bus_suffix_delim = lex_tokenstring(_l)[0];
    lex_getsym (_l);
  }
  return true;
}


bool Spef::_read_units ()
{
  double val;
  if (!lex_have (_l, _star_t_unit)) {
    spef_warning (_l, "*T_UNIT missing");
    return false;
  }
  if (!lex_have_number (_l, &val)) {
    spef_warning (_l, "*T_UNIT expected number");
    return false;
  }
  if (val < 0) {
    spef_warning (_l, "*T_UNIT expected positive number");
    return false;
  }
  if (lex_have_keyw (_l, "NS")) {
    val = val*1e-9;
  }
  else if (lex_have_keyw (_l, "PS")) {
    val = val*1e-12;
  }
  else {
    spef_warning (_l, "*T_UNIT expected NS or PS");
    return false;
  }
  _time_unit = val;

  if (!lex_have (_l, _star_c_unit)) {
    spef_warning (_l, "*C_UNIT missing");
    return false;
  }
  if (!lex_have_number (_l, &val)) {
    spef_warning (_l, "*C_UNIT expected number");
    return false;
  }
  if (val < 0) {
    spef_warning (_l, "*C_UNIT expected positive number");
    return false;
  }
  if (lex_have_keyw (_l, "PF")) {
    val = val*1e-12;
  }
  else if (lex_have_keyw (_l, "FF")) {
    val = val*1e-15;
  }
  else {
    spef_warning (_l, "*C_UNIT expected PF or FF");
    return false;
  }
  _c_unit = val;


  if (!lex_have (_l, _star_r_unit)) {
    spef_warning (_l, "*R_UNIT missing");
    return false;
  }
  if (!lex_have_number (_l, &val)) {
    spef_warning (_l, "*R_UNIT expected number");
    return false;
  }
  if (val < 0) {
    spef_warning (_l, "*R_UNIT expected positive number");
    return false;
  }
  if (lex_have_keyw (_l, "OHM")) {
    /* nothing */
  }
  else if (lex_have_keyw (_l, "KOHM")) {
    val = val*1e3;
  }
  else {
    spef_warning (_l, "*R_UNIT expected OHM or KOHM");
    return false;
  }
  _r_unit = val;

  if (!lex_have (_l, _star_l_unit)) {
    spef_warning (_l, "*L_UNIT missing");
    return false;
  }
  if (!lex_have_number (_l, &val)) {
    spef_warning (_l, "*L_UNIT expected number");
    return false;
  }
  if (val < 0) {
    spef_warning (_l, "*L_UNIT expected positive number");
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
    spef_warning (_l, "*L_UNIT expected HENRY or MH or UH");
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
      spef_warning (_l, "space after *, ignoring");
    }
    if (lex_sym (_l) == l_integer) {
      /* we're good */
      b = ihash_lookup (_nH, lex_integer (_l));
      if (b) {
	spef_warning (_l, "duplicate integer; using latest map");
      }
      else {
	b = ihash_add (_nH, lex_integer (_l));
	b->v = NULL;
      }
      lex_getsym (_l);
    }
    else {
      spef_warning (_l, "missing integer after * in name map");
      return false;
    }

    b->v = _getTokPhysicalRef ();
    if (!b->v) {
      b->v = _getTokPath ();
    }
    if (!b->v) {
      spef_warning (_l, "error parsing name");
      return false;
    }
#if 0
    printf (">> %d maps to ", (int)b->key);
    MAP_GET_PTR(b->v)->Print (stdout);
    printf ("\n");
#endif
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
      spef_warning (_l, "*POWER_NETS error");
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
      spef_warning (_l, "*POWER_NETS error");
      return false;
    }
  }

  return true;
}

bool Spef::_getPinPortInternal (spef_node *n)
{
  ActId *tmp, *tmp2;

  lex_push_position (_l);

  if ((tmp = _getIndex()) || (tmp = _getTokPath()) || (tmp = _getTokName())) {
    if (_tok_pin_delim != -1 && lex_have (_l, _tok_pin_delim)) {
      tmp2 = _getIndex();
      if (!tmp2) {
	tmp2 = _getTokPath ();
      }
      if (!tmp2) {
	lex_set_position (_l);
	lex_pop_position (_l);
	spef_warning (_l, "port name error");
	return false;
      }
      n->inst = tmp;
      n->pin = tmp2;
    }
    else {
      tmp2 = MAP_GET_PTR (tmp);
      if (tmp2->Rest() && !_a) {
	lex_set_position (_l);
	lex_pop_position (_l);
	spef_warning (_l, "port name error");
	return false;
      }
      n->inst = NULL;
      n->pin = tmp;
    }
    if (_tok_pin_delim != -1 && lex_have (_l, _tok_pin_delim)) {
      if (lex_sym (_l) == l_integer) {
	Assert (n->inst == NULL, "What?");
	n->inst = n->pin;
	n->pin = new ActId (lex_tokenstring (_l));
	lex_getsym (_l);
      }
    }
    lex_pop_position (_l);
    return true;
  }
  lex_set_position (_l);
  lex_pop_position (_l);
  return false;
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
	return false;
      }

      *inst_name = tmp;
      *port = tmp2;

      lex_pop_position (_l);
      return true;
    }
    else {
      tmp2 = MAP_GET_PTR (tmp);
      if (tmp2->Rest() && !_a) {
	lex_set_position (_l);
	lex_pop_position (_l);
	spef_warning (_l, "port name error3");
	return false;
      }
      *inst_name = NULL;
      *port = tmp;
      lex_pop_position (_l);
      return true;
    }
  }
  lex_set_position (_l);
  lex_pop_position (_l);
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
	  spef_warning (_l, "direction error");
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
	spef_warning (_l, "unexpected error");
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
	A_NEXT (_defines).design_name = NULL;
	A_INC (_defines);
      }
      if (idx == A_LEN (_defines)) {
	spef_warning (_l, "*DEFINE error");
	return false;
      }
      char *str;
      if (lex_have (_l, l_string)) {
	str = _prevString();
      }
      else {
	spef_warning (_l, "*DEFINE error");
	return false;
      }
      while (idx < A_LEN (_defines)) {
	if (str) {
	  _defines[idx].design_name = str;
	  str = NULL;
	}
	else {
	  _defines[idx].design_name = Strdup (_defines[idx-1].design_name);
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
	A_NEXT (_defines).design_name = NULL;
	A_NEXT (_defines).spef = NULL;
      }
      else {
	spef_warning (_l, "*PDEFINE error");
	return false;
      }
      if (lex_have (_l, l_string)) {
	A_NEXT (_defines).design_name = _prevString();
	A_INC (_defines);
      }
      else {
	A_INC (_defines);
	spef_warning (_l, "*PDEFINE error");
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

// custom hash functions
static int idhash (int sz, void *key)
{
  ActId *id = (ActId *) key;
  return id->getHash (0, sz);
}

static int idmatch (void *k1, void *k2)
{
  ActId *id1 = (ActId *) k1;
  ActId *id2 = (ActId *) k2;
  return id1->isEqual (id2);
}

static void *iddup (void *k)
{
  ActId *id = (ActId *)k;
  // don't clone. 
  return k;
}

static void idfree (void *k)
{
  ActId *id = (ActId *) k;
  delete id;
}

static void idprint (FILE *fp, void *k)
{
  // nothing
}

struct cHashtable *idhash_new (int sz)
{
  struct cHashtable *cH = chash_new (4);
  cH->hash = idhash;
  cH->match = idmatch;
  cH->dup = iddup;
  cH->free = idfree;
  cH->print = idprint;
  return cH;
}

bool Spef::_read_internal_def ()
{
  bool found = false;
  spef_net *net;

  _nets = idhash_new (4);
  
  while (lex_sym (_l) == _star_d_net || lex_sym (_l) == _star_r_net ||
	 lex_sym (_l) == _star_d_pnet || lex_sym (_l) == _star_r_pnet) {
    bool phys = false;
    chash_bucket_t *cb;

    found = true;

    net = new spef_net();

    if (lex_sym (_l) == _star_d_net) {
      net->type = 0;
    }
    else if (lex_sym (_l) == _star_d_pnet) {
      net->type = 2;
    }
    else if (lex_sym (_l) == _star_r_net) {
      net->type = 1;
    }
    else {
      net->type = 3;
    }

    if (lex_sym (_l) == _star_d_pnet || lex_sym (_l) == _star_r_pnet) {
      phys = true;
    }

    if (lex_have (_l, _star_d_net) || lex_have (_l, _star_d_pnet)) {
      if (!((net->net = _getIndex()) || (!phys && (net->net = _getTokPath()))
	    || (phys && (net->net = _getTokPhysicalRef())))) {
	spef_warning (_l, "*D_NET error");
	delete net;
	return false;
      }
      if (!_getParasitics (&net->tot_cap)) {
	spef_warning (_l, "*D_NET cap error");
	delete net;
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
      net->routing_confidence = -1;
      if (lex_have (_l, _star_v)) {
	if (lex_sym (_l) == l_integer) {
	  net->routing_confidence = lex_integer (_l);
	  lex_getsym (_l);
	}
	else {
	  spef_warning (_l, "*D_NET routing confidence error");
	  delete net;
	  return false;
	}
      }

      if (lex_have (_l, _star_conn)) {
	/* optional connection section */
	/* *P or *I parts */
	bool found = false;
	
	while (lex_sym (_l) == _star_p || lex_sym (_l) == _star_i) {
	  spef_conn *conn;

	  A_NEW (net->u.d.conn, spef_conn);
	  conn = &A_NEXT (net->u.d.conn);
	  A_INC (net->u.d.conn);
	  conn->a = NULL;
	  conn->inst = NULL;
	  conn->pin = NULL;
	  
	  ActId *inst = NULL, *pin = NULL;
	  int dir;
	  found = true;
	  if (lex_have (_l, _star_p)) {
	    /* port name or pport name */
	    if (!(!phys && _getPortName (false, &inst, &pin)) &&
		!_getPortName (true, &inst, &pin)) {
	      spef_warning (_l, "*P missing port");
	      delete net;
	      return false;
	    }
	    conn->type = 0;
	  }
	  else if (lex_have (_l, _star_i)) {
	    /* pin name or pnode ref */
	    if ((inst = _getIndex()) ||
		(!phys && (inst = _getTokPath()))) {
	      /* pin delim */
	      if (!lex_have (_l, _tok_pin_delim)) {
		spef_warning (_l, "*I pin error");
		delete net;
		return false;
	      }
	      pin = _getIndex();
	      if (!pin) {
		if (!phys) {
		  pin = _getTokPath();
		}
		else {
		  pin = _getTokPhysicalRef();
		}
		if (!pin) {
		  spef_warning (_l, "*I pin error");
		  delete net;
		  return false;
		}
	      }
	    }
	    else if ((inst = _getTokPhysicalRef ())) {
	      if (!lex_have (_l, _tok_pin_delim)) {
		spef_warning (_l, "*I pin error");
		delete net;
		return false;
	      }
	      pin = _getIndex();
	      if (!pin) {
		if (!phys) {
		  pin = _getTokName();
		}
		else {
		  pin = _getTokPhysicalRef ();
		}
		if (!pin) {
		  spef_warning (_l, "*I pin error");
		  delete net;
		  return false;
		}
	      }
	    }
	    else {
	      spef_warning (_l, "*I pin error");
	      delete net;
	      return false;
	    }
	    Assert (pin, "Hmm?");
	    if (MAP_GET_PTR(pin)->Rest() && !_a) {
	      spef_warning (_l, "pin error");
	      delete net;
	      return false;
	    }
	    conn->type = 1;
	  }
	  else {
	    Assert (0, "What?!");
	  }
	  conn->inst = inst;
	  conn->pin = pin;
	  dir = lex_get_dir (_l);
	  if (dir == -1) {
	    spef_warning (_l, "*CONN direction error");
	    delete net;
	    return false;
	  }
	  conn->dir = dir;
	  conn->a  = _getAttributes ();
	}
	if (!found) {
	  spef_warning (_l, "*CONN missing a conn_def");
	  delete net;
	  return false;
	}
	
	/* *N stuff */
	while (lex_have (_l, _star_n)) {
	  spef_conn *conn;

	  A_NEW (net->u.d.conn, spef_conn);
	  conn = &A_NEXT (net->u.d.conn);
	  A_INC (net->u.d.conn);
	  conn->a = NULL;
	  conn->inst = NULL;
	  conn->pin = NULL;
	  conn->type = 2;
	  
	  /* net-ref */
	  ActId *tmp;
	  if (!(tmp = _getIndex()) && !(tmp = _getTokPath())) {
	    spef_warning (_l, "*N internal node error");
	    delete net;
	    return false;
	  }
	  conn->inst = tmp;
	  if (!lex_have (_l, _tok_pin_delim)) {
	    spef_warning (_l, "*N internal node error");
	    delete net;
	    return false;
	  }
	  if (!(lex_sym (_l) == l_integer)) {
	    spef_warning (_l, "*N missing integer");
	    delete net;
	    return false;
	  }
	  conn->ipin = lex_integer (_l);
	  lex_getsym (_l);
	  if (!lex_have (_l, _star_c)) {
	    spef_warning (_l, "*N missing *C");
	    delete net;
	    return false;
	  }
	  if (!lex_have_number (_l, &conn->cx)) {
	    delete net;
	    return false;
	  }
	  if (!lex_have_number (_l, &conn->cy)) {
	    delete net;
	    return false;
	  }
	}
      }
      if (lex_have (_l, _star_cap)) {
	/* optional cap section */
	while (lex_sym (_l) == l_integer) {
	  spef_parasitic *sc;
	  A_NEW (net->u.d.caps, spef_parasitic);
	  sc = &A_NEXT (net->u.d.caps);
	  A_INC (net->u.d.caps);
	  sc->n.inst = NULL;
	  sc->n.pin = NULL;
	  sc->n2.inst = NULL;
	  sc->n2.pin = NULL;

	  sc->id = lex_integer (_l);
	  lex_getsym (_l);

	  if (!_getPinPortInternal (&sc->n)) {
	    spef_warning (_l, "node error");
	    delete net;
	    return false;
	  }

	  if (lex_sym (_l) != l_integer) {
	    /* bug in the standard */
	    if (_getPinPortInternal (&sc->n2)) {
	      /* okay, coupling cap */
	    }
	  }

	  if (!_getParasitics (&sc->val)) {
	    spef_warning (_l, "error in parasitics");
	    delete net;
	    return false;
	  }
	  SKIP_SC_OPTIONAL;
	}
      }
      if (lex_have (_l, _star_res)) {
	/* optional res section */
	while (lex_sym (_l) == l_integer) {
	  spef_parasitic *sc;
	  A_NEW (net->u.d.res, spef_parasitic);
	  sc = &A_NEXT (net->u.d.res);
	  sc->n.inst = NULL;
	  sc->n.pin = NULL;
	  sc->n2.inst = NULL;
	  sc->n2.pin = NULL;

	  sc->id = lex_integer (_l);
	  lex_getsym (_l);

	  if (!_getPinPortInternal (&sc->n)) {
	    spef_warning (_l, "*RES node error");
	    delete net;
	    return false;
	  }

	  if (!_getPinPortInternal (&sc->n2)) {
	    spef_warning (_l, "*RES node error");
	    delete net;
	    return false;
	  }
	  if (!_getParasitics (&sc->val)) {
	    spef_warning (_l, "error in parasitics");
	    delete net;
	    return false;
	  }
	  A_INC (net->u.d.res);
	  SKIP_SC_OPTIONAL;
	}
      }

      if (lex_have (_l, _star_induc)) {
	/* optional induc section */

      }

      if (!lex_have (_l, _star_end)) {
	spef_warning (_l, "*D_NET missing *END");
	delete net;
	return false;
      }
    }
    else if (lex_have (_l, _star_r_net) || lex_have (_l, _star_r_pnet)) {
      if (!((net->net = _getIndex()) || (!phys && (net->net = _getTokPath()))
	    || (phys && (net->net = _getTokPhysicalRef())))) {
	spef_warning (_l, "*R_NET error");
	delete net;
	return false;
      }
      if (!_getParasitics (&net->tot_cap)) {
	spef_warning (_l, "*R_NET error");
	delete net;
	return false;
      }
      net->routing_confidence = -1;
      if (lex_have (_l, _star_v)) {
	if (lex_sym (_l) == l_integer) {
	  net->routing_confidence = lex_integer (_l);
	  lex_getsym (_l);
	}
	else {
	  spef_warning (_l, "*R_NET routing confidence error");
	  delete net;
	  return false;
	}
      }
      A_INIT (net->u.r.drivers);
      while (lex_have (_l, _star_driver)) {
	spef_reduced *rnet;
	A_NEW (net->u.r.drivers, spef_reduced);
	rnet = &A_NEXT (net->u.r.drivers);
	A_INC (net->u.r.drivers);
	rnet->driver_inst = NULL;
	rnet->pin = NULL;
	rnet->cell_type = NULL;
	A_INIT (rnet->rc);

	if (!((rnet->driver_inst = _getIndex()) ||
	      (rnet->driver_inst = _getTokPath()))) {
	  spef_warning (_l, "*R_NET driver pin error");
	  delete net;
	  return false;
	}

	if (_tok_pin_delim == -1 || !lex_have (_l, _tok_pin_delim)) {
	  spef_warning (_l, "missing pin");
	  delete net;
	  return false;
	}

	if (!((rnet->pin = _getIndex()) || (rnet->pin = _getTokPath()))) {
	  spef_warning (_l, "missing pin");
	  delete net;
	  return false;
	}

	if (!lex_have (_l, _star_cell)) {
	  spef_warning (_l, "missing *CELL");
	  delete net;
	  return false;
	}

	if (!((rnet->cell_type = _getIndex()) ||
	      (rnet->cell_type = _getTokPath()))) {
	  spef_warning (_l, "*CELL error");
	  delete net;
	  return false;
	}

	if (!lex_have (_l, _star_c2_r1_c1)) {
	  spef_warning (_l, "missing *C2_R1_C1");
	  delete net;
	  return false;
	}

	if (!_getParasitics (&rnet->c2)) {
	  spef_warning (_l, "parasitics error");
	  delete net;
	  return false;
	}
	  
	if (!_getParasitics (&rnet->r1)) {
	  spef_warning (_l, "parasitics error");
	  delete net;
	  return false;
	}
	  
	if (!_getParasitics (&rnet->c1)) {
	  spef_warning (_l, "parasitics error");
	  delete net;
	  return false;
	}

	/* loads */
	if (!lex_have (_l, _star_loads)) {
	  spef_warning (_l, "missing *LOADS");
	  delete net;
	  return false;
	}

	while (lex_have (_l, _star_rc)) {
	  spef_rc_desc *rc;
	  A_NEW (rnet->rc, spef_rc_desc);
	  rc = &A_NEXT (rnet->rc);
	  A_INC (rnet->rc);
	  rc->n.inst = NULL;
	  rc->n.pin = NULL;

	  if (!((rc->n.inst = _getIndex()) || (rc->n.inst = _getTokPath()))) {
	    spef_warning (_l, "missing pin name for *RC");
	    delete net;
	    return false;
	  }
	  if (_tok_pin_delim == -1 || !lex_have (_l, _tok_pin_delim)) {
	    spef_warning (_l, "missing pin");
	    delete net;
	    return false;
	  }
	  if (!((rc->n.pin = _getIndex()) || (rc->n.pin = _getTokPath()))) {
	    spef_warning (_l, "missing pin name for *RC");
	    delete net;
	    return false;
	  }

	  if (!_getParasitics (&rc->val)) {
	    spef_warning (_l, "missing parastics");
	    delete net;
	    return false;
	  }

	  if (lex_have (_l, _star_q)) {
	    if (lex_sym (_l) != l_integer) {
	      spef_warning (_l, "missing index");
	      delete net;
	      return false;
	    }
	    rc->pole.idx = lex_integer (_l);
	    lex_getsym (_l);

	    if (!_getComplexParasitics (&rc->pole.re, &rc->pole.im)) {
	      spef_warning (_l, "parasitics error");
	      delete net;
	      return false;
	    }

	    if (!lex_have (_l, _star_k)) {
	      spef_warning (_l, "missing residue");
	      delete net;
	      return false;
	    }
	    if (lex_sym (_l) != l_integer) {
	      spef_warning (_l, "missing index");
	      delete net;
	      return false;
	    }
	    rc->residue.idx = lex_integer (_l);
	    lex_getsym (_l);
	    
	    if (!_getComplexParasitics (&rc->residue.re, &rc->residue.im)) {
	      spef_warning (_l, "parasitics error");
	      delete net;
	      return false;
	    }
	  }
	  else {
	    rc->pole.idx = -1;
	    rc->residue.idx = -1;
	  }
	}
      }
      if (!lex_have (_l, _star_end)) {
	spef_warning (_l, "*R_NET missing *END");
	delete net;
	return false;
      }
    }
    else {
      delete net;
      net = NULL;
    }
    if (net) {
      if (chash_lookup (_nets, MAP_GET_PTR (net->net))) {
	warning ("Duplicate net found; skipped!");
	delete net;
      }
      else {
	cb = chash_add (_nets, MAP_GET_PTR (net->net));
	cb->v = net;
      }
    }
  }
  return found;
}

static int _valid_escaped_chars (char c)
{
  if (c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' ||
      c == '(' || c == ')' || c == '*' || c == '+' || c == ',' || c == '-' ||
      c == '+' || c == ',' || c == '-' || c == '.' || c == '/' || c == ':' ||
      c == ';' || c == '<' || c == '=' || c == '>' || c == '?' || c == '@' ||
      c == '[' || c == '\\' || c == ']' ||c == '^' || c == '`' || c == '{' ||
      c == '}' || c == '~' || c == '"') {
    return 1;
  }
  return 0;
}

static int _valid_id_chars (char *s)
{
  while (*s) {
    if (((*s) >= 'a' && (*s) <= 'z') || ((*s) >= 'A' && (*s) <= 'Z') ||
	((*s) >= '0' && (*s) <= '9') ||	((*s) == '_')) {
      s++;
    }
    else if ((*s) == '\\' && _valid_escaped_chars (*(s+1))) {
      s += 2;
    }
    else {
      return 0;
    }
  }
  return 1;
}


#define _valid_bus_chars(c) (*(c) == _bus_prefix_delim || *(c) == _bus_suffix_delim)
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

  int off = 0;

  /* next token is ID */
  while (_valid_id_chars (lex_tokenstring (_l) + off) ||
	 _valid_bus_chars (lex_tokenstring (_l) + off)) {
    while (strlen (lex_tokenstring (_l)) + buflen + 1 > bufsz) {
      bufsz *= 2;
      REALLOC (curbuf, char, bufsz);
    }
    snprintf (curbuf + buflen, bufsz - buflen, "%s", lex_tokenstring (_l));
    buflen += strlen (curbuf + buflen);
    lex_getsym (_l);
    if (strcmp (lex_whitespace (_l), "") != 0) {
      break;
    }
    while (strcmp (lex_tokenstring (_l), "\\") == 0) {
      lex_getsym (_l);
      if (strcmp (lex_whitespace (_l), "") != 0) {
	FREE (curbuf);
	return NULL;
      }
      if (!_valid_escaped_chars (lex_tokenstring (_l)[0])) {
	FREE (curbuf);
	return NULL;
      }
      if (buflen + 2 > bufsz) {
	bufsz *= 2;
	REALLOC (curbuf, char, bufsz);
      }
      // stuff the escaped character
      snprintf (curbuf + buflen, bufsz - buflen, "%c", lex_tokenstring(_l)[0]);
      buflen++;
      if (lex_tokenstring(_l)[1]) {
	off = 1;
	break;
      }
      else {
	off = 0;
	lex_getsym (_l);
      }
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

static bool _has_dot (const char *s)
{
  while (*s) {
    if (*s == '.') return 1;
    s++;
  }
  return 0;
}


ActId *Spef::_strToId (char *s)
{
  ActId *ret;
  
  if (_a) {
    char *tmpbuf;
    int len = strlen (s) + 1;
    MALLOC (tmpbuf, char, len);
    if (_has_dot (s)) {
      snprintf (tmpbuf, len, "%s", s);
    }
    else {
      _a->unmangle_string (s, tmpbuf, len);
    }
#if 0
    fprintf (stderr, " ==> orig: %s\n", s);
    fprintf (stderr, "   -> new: %s\n", tmpbuf);
#endif
    ret = ActId::parseId (tmpbuf, '.', '[', ']', '.');
    FREE (tmpbuf);
#if 0
    if (!ret) {
      fprintf (stderr, "   -> err!\n\n");
      return NULL;
    }
    else {
      fprintf (stderr, "   -> ret: ");
      ret->Print (stderr);
      fprintf (stderr, "\n\n");
    }
#endif
  }
  else {
    ret = new ActId (string_cache (s));
  }
  return ret;
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
  tmp = _strToId (s);
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
      ret = _strToId (part);
      tmp = ret->Tail();
    }
    else {
      tmp->Append (_strToId (part));
      tmp = tmp->Tail();
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

  if (_a) {
    if (lex_sym (_l) != l_id && lex_sym (_l) != l_integer) {
      lex_set_position (_l);
      lex_pop_position (_l);
      return NULL;
    }
    ret = _strToId (lex_tokenstring (_l));

    if (!ret) {
      lex_set_position (_l);
      lex_pop_position (_l);
      return NULL;
    }
    lex_getsym (_l);
  }
  else {
    /* normal SPEF */
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
      if (!(lex_sym (_l) == l_integer)) {
	delete ret;
	lex_set_position (_l);
	lex_pop_position (_l);
	return NULL;
      }
      Array *a = new Array (lex_integer (_l));
      lex_getsym (_l);
      tmp->setArray (a);
      if (_tok_suffix_bus_delim != -1 && lex_have (_l, _tok_suffix_bus_delim)) {
	/* nothing */
      }
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
      int ival = lex_integer (_l);
      ihash_bucket_t *b;
      lex_getsym (_l);
      if (!_nH) {
	lex_set_position (_l);
	lex_pop_position (_l);
	return NULL;
      }
      b = ihash_lookup (_nH, ival);
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

bool Spef::getParasitics (LEX_T *l, int colon, spef_triplet *t)
{
  lex_push_position (l);

  if (!lex_have_number (l, &t->typ)) {
    lex_set_position (l);
    lex_pop_position (l);
    return false;
  }

  if (!lex_have (l, colon)) {
    t->best = t->typ;
    t->worst = t->typ;
    lex_pop_position (l);
    return true;
  }
  t->best = t->typ;
  if (!lex_have_number (l, &t->typ)) {
    //lex_set_position (l);
    //lex_pop_position (l);
    //return false;
    t->typ = t->best;
  }
  if (!lex_have (l, colon)) {
    lex_set_position (l);
    lex_pop_position (l);
    return false;
  }
  if (!lex_have_number (l, &t->worst)) {
    //lex_set_position (l);
    //lex_pop_position (l);
    //return false;
    t->worst = t->typ;
  }
  lex_pop_position (l);
  return true;
}

bool Spef::_getParasitics (spef_triplet *t)
{
  return getParasitics (_l, _tok_colon, t);
}

bool Spef::_getComplexParasitics (spef_triplet *re, spef_triplet *im)
{
  lex_push_position (_l);

  if (!lex_have_number (_l, &re->typ)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }

  if (lex_have (_l, _tok_colon)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    im->best = 0;
    im->typ = 0;
    im->worst = 0;
    return _getParasitics (re);
  }

  if (!lex_have_number (_l, &im->typ)) {
    re->best = re->typ;
    re->worst = re->typ;
    im->best = 0;
    im->worst = 0;
    im->typ = 0;
    lex_pop_position (_l);
    return true;
  }

  if (!lex_have (_l, _tok_colon)) {
    re->best = re->typ;
    re->worst = re->typ;
    im->best = im->typ;
    im->worst = im->typ;
    lex_pop_position (_l);
    return true;
  }
  re->best = re->typ;
  im->best = im->typ;

  if (!lex_have_number (_l, &re->typ)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }
  if (!lex_have_number (_l, &im->typ)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }
  
  if (!lex_have (_l, _tok_colon)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }
  
  if (!lex_have_number (_l, &re->worst)) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return false;
  }
  if (!lex_have_number (_l, &im->worst)) {
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
	spef_warning (_l, "duplicate *L");
      }
      ret->load = 1;

      if (!_getParasitics (&ret->l)) {
	spef_warning (_l, "parasitics error");
	FREE (ret);
	return NULL;
      }
    }
    else if (lex_have (_l, _star_c)) {
      if (ret->coord) {
	spef_warning (_l, "duplicate *C");
      }
      ret->coord = 1;
      if (!lex_have_number (_l, &ret->cx)) {
	spef_warning (_l, "parasitics error");
	FREE (ret);
	return NULL;
      }
      if (!lex_have_number (_l, &ret->cy)) {
	spef_warning (_l, "parasitics error");
	FREE (ret);
	return NULL;
      }
    }
    else if (lex_have (_l, _star_s)) {
      if (ret->slew) {
	spef_warning (_l, "duplicate *S");
      }
      ret->slew = 1;
      if (_getParasitics (&ret->s1) && _getParasitics (&ret->s2)) {
	if (_getParasitics (&ret->t1)) {
	  ret->slewth = 1;
	  if (!_getParasitics (&ret->t2)) {
	    spef_warning (_l, "parasitics error");
	  }
	}
      }
      else {
	spef_warning (_l, "parasitics error");
	FREE (ret);
	return NULL;
      }
    }
    else if (lex_have (_l, _star_d)) {
      if (ret->drive) {
	spef_warning (_l, "duplicate *D");
      }
      ActId *tmp;
      tmp = _getIndex ();
      if (!tmp) {
	tmp = _getTokPath ();
      }
      if (!tmp) {
	spef_warning (_l, "parasitics error");
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

static void _print_triplet (FILE *fp, spef_triplet *t)
{
  if (t->best == t->worst && t->best == t->typ) {
    fprintf (fp, "%g", t->typ);
  }
  else {
    fprintf (fp, "%g:%g:%g", t->best, t->typ, t->worst);
  }
}

static void _print_triplet_complex (FILE *fp, spef_triplet *re, spef_triplet *im)
{
  if (im->typ == 0 && im->worst == 0 && im->best == 0) {
    _print_triplet (fp, re);
  }
  else {
    if (im->typ == im->worst && im->typ == im->best &&
	re->typ == re->worst && re->typ == re->best) {
      fprintf (fp, "%g %g", re->typ, im->typ);
    }
    else {
      fprintf (fp, "%g %g:%g %g:%g %g",
	       re->best, im->best,
	       re->typ, im->typ,
	       re->worst, im->worst);
    }
  }
}

static void _print_attributes (FILE *fp, spef_attributes *a)
{
  if (a->coord) {
    fprintf (fp, " *C %g %g", a->cx, a->cy);
  }
  if (a->load) {
    fprintf (fp, " *L ");
    _print_triplet (fp, &a->l);
  }
  if (a->slew) {
    fprintf (fp, " *S ");
    _print_triplet (fp, &a->s1);
    fprintf (fp, " ");
    _print_triplet (fp, &a->s2);
    if (a->slewth) {
      fprintf (fp, " ");
      _print_triplet (fp, &a->t2);
    }
  }
  if (a->drive) {
    fprintf (fp, " *D ");
    if (a->cell) {
      MAP_GET_PTR(a->cell)->Print (fp);
    }
  }
}

static void _print_spef_port (FILE *fp, spef_ports *p, char _delimiter)
{
  if (p->inst && p->port) {
    MAP_GET_PTR (p->inst)->Print (fp);
    fprintf (fp, "%c", _delimiter);
    MAP_GET_PTR(p->port)->Print (fp);
  }
  else {
    Assert (p->port && !p->inst, "What?");
    MAP_GET_PTR(p->port)->Print (fp);
  }
  if (p->dir == 0) {
    fprintf (fp, " I");
  }
  else if (p->dir == 1) {
    fprintf (fp, " O");
  }
  else {
    fprintf (fp, " B");
  }
  if (p->a) {
    _print_attributes (fp, p->a);
  }
}

void Spef::Print (FILE *fp)
{
  auto lambda = [fp] (const char *s, const char *v)
    {
      if (v) {
	fprintf (fp, "%s \"%s\"\n", s, v);
      }
    };

  if (!_valid) {
    fprintf (fp, "/* WARNING: invalid spef! */\n");
  }

  lambda("*SPEF", _spef_version);
  lambda("*DESIGN", _design_name);
  lambda("*DATE", _date);
  lambda("*VENDOR", _vendor);
  lambda("*PROGRAM", _program);
  lambda("*VERSION", _version);

  lambda ("*DESIGN_FLOW", "-not-recorded-");

  fprintf (fp, "*DIVIDER %c\n", _divider);
  fprintf (fp, "*DELIMITER %c\n", _delimiter);
  fprintf (fp, "*BUS_DELIMITER %c", _bus_prefix_delim);
  if (_tok_suffix_bus_delim != -1) {
    fprintf (fp, " %c", _bus_suffix_delim);
  }
  fprintf (fp, "\n");
  
  if (_time_unit >= 1e-9) {
    fprintf (fp, "*T_UNIT %g NS\n", _time_unit*1e9);
  }
  else {
    fprintf (fp, "*T_UNIT %g PS\n", _time_unit*1e12);
  }

  if (_c_unit >= 1e-12) {
    fprintf (fp, "*C_UNIT %g PF\n", _c_unit*1e12);
  }
  else {
    fprintf (fp, "*C_UNIT %g FF\n", _c_unit*1e15);
  }

  if (_r_unit >= 1e3) {
    fprintf (fp, "*R_UNIT %g KOHM\n", _r_unit*1e-3);
  }
  else {
    fprintf (fp, "*R_UNIT %g OHM\n", _r_unit);
  }

  if (_l_unit >= 1) {
    fprintf (fp, "*L_UNIT %g HENRY\n", _l_unit);
  }
  else if (_l_unit >= 1e-3) {
    fprintf (fp, "*L_UNIT %g MH\n", _l_unit*1e3);
  }
  else {
    fprintf (fp, "*L_UNIT %g UH\n", _l_unit*1e6);
  }

  /* name map */
  if (_nH) {
    ihash_iter_t it;
    ihash_bucket_t *b;
    fprintf (fp, "*NAME_MAP\n");
    ihash_iter_init (_nH, &it);
    while ((b = ihash_iter_next (_nH, &it))) {
      ActId *id;
      fprintf (fp, "*%lu ", b->key);
      id = MAP_GET_PTR (b->v);
      if (MAP_IS_ABS (b->v)) {
	fprintf (fp, "%c", _divider);
      }
      id->Print (fp);
      fprintf (fp, "\n");
    }
  }

  /* power def */
  if (A_LEN (_power_nets) > 0) {
    fprintf (fp, "*POWER_NETS");
    for (int i=0; i < A_LEN (_power_nets); i++) {
      fprintf (fp, " ");
      MAP_GET_PTR(_power_nets[i])->Print (fp);
    }
    fprintf (fp, "\n");
  }

  if (A_LEN (_gnd_nets) > 0) {
    fprintf (fp, "*GND_NETS");
    for (int i=0; i < A_LEN (_gnd_nets); i++) {
      fprintf (fp, " ");
      MAP_GET_PTR(_gnd_nets[i])->Print (fp);
    }
    fprintf (fp, "\n");
  }

  if (A_LEN (_ports) > 0) {
    fprintf (fp, "*PORTS\n");
    for (int i=0; i < A_LEN (_ports); i++) {
      _print_spef_port (fp, &_ports[i], _delimiter);
      fprintf (fp, "\n");
    }
  }
  if (A_LEN (_phyports) > 0) {
    fprintf (fp, "*PPORTS\n");
    for (int i=0; i < A_LEN (_phyports); i++) {
      _print_spef_port (fp, &_phyports[i], _delimiter);
      fprintf (fp, "\n");
    }
  }

  if (A_LEN (_defines) > 0) {
    for (int i=0; i < A_LEN (_defines); i++) {
      if (_defines[i].phys) {
	fprintf (fp, "*PDEFINE");
      }
      else {
	fprintf (fp, "*DEFINE");
      }
      if (_defines[i].inst) {
	fprintf (fp, " ");
	MAP_GET_PTR(_defines[i].inst)->Print (fp);
      }
      if (_defines[i].design_name) {
	fprintf (fp, " \"%s\"", _defines[i].design_name);
      }
      fprintf (fp, "\n");
    }
  }

  if (_nets && _nets->n > 0) {
    chash_iter_t it;
    chash_bucket_t *cb;
    chash_iter_init (_nets, &it);
    while ((cb = chash_iter_next (_nets, &it))) {
      spef_net *n = (spef_net *) cb->v;
      n->Print (this, fp);
    }
  }
}



bool SpefCollection::ReadExt (const char *name)
{
  struct ext_file *e;

  ext_validate_timestamp (name);
  e = ext_read (name);

  /* 
   * convert to SPEF data structure
   */
  return true;
}


void spef_net::Print (Spef *S, FILE *fp)
{
  if (type == 0) {
    fprintf (fp, "*D_NET ");
  }
  else if (type == 1) {
    fprintf (fp, "*R_NET ");
  }
  else if (type == 2) {
    fprintf (fp, "*D_PNET ");
  }
  else {
    fprintf (fp, "*R_PNET ");
  }

  char pin_delim = S->getPinDivider ();

  // this is not the *<NUM> format!
  MAP_GET_PTR(net)->Print (fp);
  
  fprintf (fp, " ");
  _print_triplet (fp, &tot_cap);
  if (routing_confidence != -1) {
    fprintf (fp, " %d", routing_confidence);
  }
  fprintf (fp, "\n");

  if (type == 1 || type == 3) {
    for (int i=0; i < A_LEN (u.r.drivers); i++) {
      spef_reduced *r = u.r.drivers + i;
      fprintf (fp, "*DRIVER ");
      if (r->driver_inst) {
	MAP_GET_PTR(r->driver_inst)->Print (fp);
	fprintf (fp, "%c", pin_delim);
      }
      MAP_GET_PTR(r->pin)->Print (fp);
      fprintf (fp, "\n");
      fprintf (fp, "*CELL ");
      MAP_GET_PTR(r->cell_type)->Print (fp);
      fprintf (fp, "\n");
      fprintf (fp, "*C2_R1_C1 ");
      _print_triplet (fp, &r->c2);
      fprintf (fp, " ");
      _print_triplet (fp, &r->r1);
      fprintf (fp, " ");
      _print_triplet (fp, &r->c1);
      fprintf (fp, "\n*LOADS\n");
      for (int j=0; j < A_LEN (r->rc); j++) {
	fprintf (fp, "*RC ");
	if (r->rc[j].n.inst) {
	  MAP_GET_PTR(r->rc[j].n.inst)->Print (fp);
	  fprintf (fp, "%c", pin_delim);
	}
	MAP_GET_PTR(r->rc[j].n.pin)->Print (fp);
	fprintf (fp, " ");
	_print_triplet (fp, &r->rc[j].val);
	fprintf (fp, "\n");
	if (r->rc[j].pole.idx != -1) {
	  fprintf (fp, "*Q %d ", r->rc[j].pole.idx);
	  _print_triplet_complex (fp, &r->rc[j].pole.re, &r->rc[j].pole.im);
	  fprintf (fp, "\n");
	}
	if (r->rc[j].residue.idx != -1) {
	  fprintf (fp, "*K %d ", r->rc[j].residue.idx);
	  _print_triplet_complex (fp, &r->rc[j].residue.re, &r->rc[j].residue.im);
	  fprintf (fp, "\n");
	}
      }
    }
    // R_NET
    // driver_reduc
  }
  else {
    // D_NET
    //  conn_sec cap_sec res_sec induc_sec *END

    if (A_LEN (u.d.conn) > 0) {
      fprintf (fp, "*CONN\n");
    }
    for (int i=0; i < A_LEN (u.d.conn); i++) {
      spef_conn *c = u.d.conn + i;
      if (c->type == 0) {
	fprintf (fp, "*P ");
      }
      else if (c->type == 1) {
	fprintf (fp, "*I ");
      }
      else if (c->type == 2) {
	fprintf( fp, "*N ");
      }
      else {
	Assert (0, "Invalid conn type");
      }
      if (c->inst) {
	MAP_GET_PTR(c->inst)->Print (fp);
	fprintf (fp, "%c", pin_delim);
	MAP_GET_PTR(c->pin)->Print (fp);
      }
      else {
	MAP_GET_PTR(c->pin)->Print (fp);
      }
      if (c->type == 2) {
	fprintf (fp, "%c%d %g %g\n", pin_delim, c->ipin, c->cx, c->cy);
      }
      else {
	if (c->dir == 0) {
	  fprintf (fp, " I");
	}
	else if (c->dir == 1) {
	  fprintf (fp, " O");
	}
	else {
	  Assert (c->dir == 2, "What?");
	  fprintf (fp, " B");
	}
	if (c->a) {
	  _print_attributes (fp, c->a);
	}
      }
      fprintf (fp, "\n");
    }

    if (A_LEN (u.d.caps) > 0) {
      fprintf (fp, "*CAP\n");
      for (int i=0; i < A_LEN (u.d.caps); i++) {
	u.d.caps[i].Print (fp, pin_delim);
	fprintf (fp, "\n");
      }
    }
    if (A_LEN (u.d.res) > 0) {
      fprintf (fp, "*RES\n");
      for (int i=0; i < A_LEN (u.d.res); i++) {
	u.d.res[i].Print (fp, pin_delim);
	fprintf (fp, "\n");
      }
    }
    if (A_LEN (u.d.induc) > 0) {
      fprintf (fp, "*INDUC\n");
      for (int i=0; i < A_LEN (u.d.induc); i++) {
	u.d.induc[i].Print (fp, pin_delim);
	fprintf (fp, "\n");
      }
    }
  }
  fprintf (fp, "*END\n");
}

void spef_net::spPrint (Spef *S, FILE *fp)
{
  if (type == 0) {
    fprintf (fp, "*D_NET ");
  }
  else if (type == 1) {
    fprintf (fp, "*R_NET ");
  }
  else if (type == 2) {
    fprintf (fp, "*D_PNET ");
  }
  else {
    fprintf (fp, "*R_PNET ");
  }

  char pin_delim = S->getPinDivider ();

  // this is not the *<NUM> format!
  MAP_GET_PTR(net)->Print (fp);

  fprintf (fp, "\n");

#if 0  
  fprintf (fp, " ");
  _print_triplet (fp, &tot_cap);
  if (routing_confidence != -1) {
    fprintf (fp, " %d", routing_confidence);
  }
  fprintf (fp, "\n");
#endif  

  if (type == 1 || type == 3) {
    // XXX: ignore this for the moment
#if 0    
    for (int i=0; i < A_LEN (u.r.drivers); i++) {
      spef_reduced *r = u.r.drivers + i;
      fprintf (fp, "*DRIVER ");
      if (r->driver_inst) {
	MAP_GET_PTR(r->driver_inst)->Print (fp);
	fprintf (fp, "%c", pin_delim);
      }
      MAP_GET_PTR(r->pin)->Print (fp);
      fprintf (fp, "\n");
      fprintf (fp, "*CELL ");
      MAP_GET_PTR(r->cell_type)->Print (fp);
      fprintf (fp, "\n");
      fprintf (fp, "*C2_R1_C1 ");
      _print_triplet (fp, &r->c2);
      fprintf (fp, " ");
      _print_triplet (fp, &r->r1);
      fprintf (fp, " ");
      _print_triplet (fp, &r->c1);
      fprintf (fp, "\n*LOADS\n");
      for (int j=0; j < A_LEN (r->rc); j++) {
	fprintf (fp, "*RC ");
	if (r->rc[j].n.inst) {
	  MAP_GET_PTR(r->rc[j].n.inst)->Print (fp);
	  fprintf (fp, "%c", pin_delim);
	}
	MAP_GET_PTR(r->rc[j].n.pin)->Print (fp);
	fprintf (fp, " ");
	_print_triplet (fp, &r->rc[j].val);
	fprintf (fp, "\n");
	if (r->rc[j].pole.idx != -1) {
	  fprintf (fp, "*Q %d ", r->rc[j].pole.idx);
	  _print_triplet_complex (fp, &r->rc[j].pole.re, &r->rc[j].pole.im);
	  fprintf (fp, "\n");
	}
	if (r->rc[j].residue.idx != -1) {
	  fprintf (fp, "*K %d ", r->rc[j].residue.idx);
	  _print_triplet_complex (fp, &r->rc[j].residue.re, &r->rc[j].residue.im);
	  fprintf (fp, "\n");
	}
      }
    }
#endif    
  }
  else {
    // D_NET
    //  conn_sec cap_sec res_sec induc_sec *END
#if 0    
    if (A_LEN (u.d.conn) > 0) {
      fprintf (fp, "*CONN\n");
    }
    for (int i=0; i < A_LEN (u.d.conn); i++) {
      spef_conn *c = u.d.conn + i;
      if (c->type == 0) {
	fprintf (fp, "*P ");
      }
      else if (c->type == 1) {
	fprintf (fp, "*I ");
      }
      else if (c->type == 2) {
	fprintf( fp, "*N ");
      }
      else {
	Assert (0, "Invalid conn type");
      }
      if (c->inst) {
	MAP_GET_PTR(c->inst)->Print (fp);
	fprintf (fp, "%c", pin_delim);
	MAP_GET_PTR(c->pin)->Print (fp);
      }
      else {
	MAP_GET_PTR(c->pin)->Print (fp);
      }
      if (c->type == 2) {
	fprintf (fp, "%c%d %g %g\n", pin_delim, c->ipin, c->cx, c->cy);
      }
      else {
	if (c->dir == 0) {
	  fprintf (fp, " I");
	}
	else if (c->dir == 1) {
	  fprintf (fp, " O");
	}
	else {
	  Assert (c->dir == 2, "What?");
	  fprintf (fp, " B");
	}
	if (c->a) {
	  _print_attributes (fp, c->a);
	}
      }
      fprintf (fp, "\n");
    }

    if (A_LEN (u.d.caps) > 0) {
      fprintf (fp, "*CAP\n");
      for (int i=0; i < A_LEN (u.d.caps); i++) {
	u.d.caps[i].Print (fp, pin_delim);
	fprintf (fp, "\n");
      }
    }
    if (A_LEN (u.d.res) > 0) {
      fprintf (fp, "*RES\n");
      for (int i=0; i < A_LEN (u.d.res); i++) {
	u.d.res[i].Print (fp, pin_delim);
	fprintf (fp, "\n");
      }
    }
    if (A_LEN (u.d.induc) > 0) {
      fprintf (fp, "*INDUC\n");
      for (int i=0; i < A_LEN (u.d.induc); i++) {
	u.d.induc[i].Print (fp, pin_delim);
	fprintf (fp, "\n");
      }
    }
#endif    
  }
}



void spef_node::Print (FILE *fp, char delim)
{
  if (inst) {
    MAP_GET_PTR (inst)->Print (fp);
    fprintf (fp, "%c", delim);
    MAP_GET_PTR (pin)->Print (fp);
  }
  else {
    MAP_GET_PTR (pin)->Print (fp);
  }
}

void spef_parasitic::Print (FILE *fp, char delim)
{
  fprintf (fp, "%d ", id);
  n.Print (fp, delim);
  fprintf (fp, " ");
  if (n2.exists()) {
    n2.Print (fp, delim);
    fprintf (fp, " ");
  }
  _print_triplet (fp, &val);
}


bool Spef::isSplit (const char *s)
{
  if (!_nets) {
    return false;
  }
  char *t = Strdup (s);
  ActId *id = _strToId (t);
  FREE (t);
  if (chash_lookup (_nets, id)) {
    delete id;
    return true;
  }
  delete id;
  return false;
}

void Spef::dumpRC (FILE *fp)
{
  if (_nets && _nets->n > 0) {
    chash_iter_t it;
    chash_bucket_t *cb;
    chash_iter_init (_nets, &it);
    while ((cb = chash_iter_next (_nets, &it))) {
      spef_net *n = (spef_net *) cb->v;
      n->spPrint (this, fp);
    }
  }
}
