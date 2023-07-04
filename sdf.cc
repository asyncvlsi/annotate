/*************************************************************************
 *
 *  Copyright (c) 2023 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
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
#include "sdf.h"


/**
 * Initialize SDF reader 
 * @param mangled_ids is set to true if the IDs from the SDF came from
 * mangled characters generated by ACT
 */
SDF::SDF (bool mangled_ids)
{
  _l = NULL;
#define TOKEN(a,b) a = -1;
#include "sdf.def"
  _valid = false;

  _h.sdfversion = NULL;
  _h.designname = NULL;
  _h.date = NULL;
  _h.vendor = NULL;
  _h.program = NULL;
  _h.version = NULL;
  _h.divider = '.';
  _h.voltage.best = 0;
  _h.voltage.typ = 0;
  _h.voltage.worst = 0;
  _h.process = NULL;
  _h.temp.best = 25.0;
  _h.temp.typ = 25.0;
  _h.temp.worst = 25.0;
  _h.timescale = 1.0;

  _err_ctxt = NULL;

  A_INIT (_cells);

  if (mangled_ids) {
    _a = ActNamespace::Act();
    if (!_a) {
      _a = new Act ();
    }
  }
  else {
    _a = NULL;
  }
}


SDF::~SDF()
{
  // release storage here!
  if (_h.sdfversion) {
    FREE (_h.sdfversion);
  }
  if (_h.designname) {
    FREE (_h.designname);
  }
  if (_h.date) {
    FREE (_h.date);
  }
  if (_h.vendor) {
    FREE (_h.vendor);
  }
  if (_h.program) {
    FREE (_h.program);
  }
  if (_h.version) {
    FREE (_h.version);
  }
  if (_h.process) {
    FREE (_h.process);
  }
  
}

bool SDF::Read (const char *name)
{
  FILE *fp = fopen (name, "r");
  
  if (!fp) {
    fprintf (stderr, "SDF::Read(): Could not open file `%s'", name);
    return false;
  }

  bool ret = Read (fp);
  fclose (fp);

  return ret;
}


void SDF::_errmsg (const char *buf)
{
  char *s = lex_errstring (_l);
  if (_err_ctxt) {
    fprintf (stderr, "SDF::PARSER(): Context `%s': Expecting `%s', looking-at: %s\n%s\n",
	     _err_ctxt, buf, lex_tokenstring (_l), s);
  }
  else {
    fprintf (stderr, "SDF::PARSER(): Expecting `%s', looking-at: %s\n%s\n",
	     buf, lex_tokenstring (_l), s);
  }
  FREE (s);
}
  

bool SDF::_mustbe (int tok)
{
  if (!lex_have (_l, tok)) {
    _errmsg (lex_tokenname (_l, tok));
    return false;
  }
  return true;
}

bool SDF::Read (FILE *fp)
{
  int count = 0;
  _l = lex_file (fp);
#define TOKEN(a,b) a = lex_addtoken (_l, b);
#include "sdf.def"
  lex_getsym (_l);

  // (
  if (!_mustbe (_tok_lpar)) goto error;

  // DELAYFILE
  if (!_mustbe (_DELAYFILE)) goto error;

  // sdf header
  if (!_read_sdfheader ()) goto error;

  while (_read_cell ()) {
    count++;
  }

  // )
  if (!_mustbe (_tok_rpar)) goto error;

  if (count == 0) {
    fprintf (stderr, "SDF::PARSER(): No cells specified in SDF file!\n");
    goto error;
  }

  lex_free (_l);
  _l = NULL;
  _valid = true;
  return true;

error:
  lex_free (_l);
  _l = NULL;
  _valid = false;
  return false;
}


/*
 * Header
 */
bool SDF::_read_sdfheader ()
{
  char *tmp;
  if (!_mustbe (_tok_lpar)) return false;
  if (!_mustbe (_SDFVERSION)) return false;
  if (!_mustbe (l_string)) return false;

  if (_h.sdfversion) {
    FREE (_h.sdfversion);
  }
  _h.sdfversion = Strdup (lex_prev (_l)+1);
  _h.sdfversion[strlen (_h.sdfversion)-1] = '\0';

  if (!_mustbe (_tok_rpar)) return false;

#define PROCESS_STRING(field)			\
  do {						\
    lex_pop_position (_l);			\
    if (!_mustbe (l_string)) return false;	\
    if (_h.field) {				\
      FREE (_h.field);				\
    }						\
    _h.field = Strdup (lex_prev (_l)+1);	\
    _h.field[strlen(_h.field)-1] = '\0';	\
  } while (0)

  // Allow in any order: more general than the actual SDF standard
  // that requires these fields in a specific order.
  while (1) {
    lex_push_position (_l);
    if (lex_have (_l, _tok_lpar)) {
      if (lex_have (_l, _DESIGN)) {
	PROCESS_STRING (designname);
      }
      else if (lex_have (_l, _DATE)) {
	PROCESS_STRING (date);
      }
      else if (lex_have (_l, _VENDOR)) {
	PROCESS_STRING (vendor);
      }
      else if (lex_have (_l, _PROGRAM)) {
	PROCESS_STRING (program);
      }
      else if (lex_have (_l, _VERSION)) {
	PROCESS_STRING (version);
      }
      else if (lex_have (_l, _DIVIDER)) {
	lex_pop_position (_l);
	// one character
	if (strlen (lex_tokenstring (_l)) != 1) {
	  fprintf (stderr, "DIVIDER must be a single character!\n");
	  return false;
	}
	else {
	  _h.divider = lex_tokenstring(_l)[0];
	}
	lex_getsym (_l);
      }
      else if (lex_have (_l, _VOLTAGE)) {
	lex_pop_position (_l);
	if (!Spef::getParasitics (_l, _tok_colon, &_h.voltage)) {
	  fprintf (stderr, "VOLTAGE specifier error!\n");
	  return false;
	}
      }
      else if (lex_have (_l, _PROCESS)) {
	PROCESS_STRING (process);
      }
      else if (lex_have (_l, _TEMPERATURE)) {
	lex_pop_position (_l);
	if (!Spef::getParasitics (_l, _tok_colon, &_h.temp)) {
	  fprintf (stderr, "TEMPERATURE specifier error!\n");
	  return false;
	}
      }
      else if (lex_have (_l, _TIMESCALE)) {
	int val;
	lex_pop_position (_l);
	if (strcmp (lex_tokenstring (_l), "1") == 0 ||
	    strcmp (lex_tokenstring (_l), "1.0") == 0) {
	  val = 1;
	}
	else if (strcmp (lex_tokenstring (_l), "10") == 0 ||
		 strcmp (lex_tokenstring (_l), "10.0") == 0) {
	  val = 10;
	}
	else if (strcmp (lex_tokenstring (_l), "100") == 0 ||
		 strcmp (lex_tokenstring (_l), "100.0") == 0) {
	  val = 100;
	}
	else {
	  fprintf (stderr, "TIMESCALE specifier error!\n");
	  return false;
	}
	lex_getsym (_l);
	if (strcmp (lex_tokenstring (_l), "s") == 0) {
	  _h.timescale = val;
	}
	else if (strcmp (lex_tokenstring (_l), "ms") == 0) {
	  _h.timescale = val*1e-3;
	}
	else if (strcmp (lex_tokenstring (_l), "us") == 0) {
	  _h.timescale = val*1e-6;
	}
	else if (strcmp (lex_tokenstring (_l), "ns") == 0) {
	  _h.timescale = val*1e-9;
	}
	else if (strcmp (lex_tokenstring (_l), "ps") == 0) {
	  _h.timescale = val*1e-12;
	}
	else if (strcmp (lex_tokenstring (_l), "fs") == 0) {
	  _h.timescale = val*1e-15;
	}
	else {
	  fprintf (stderr, "TIMESCALE specifier error!\n");
	}
	lex_getsym (_l);
      }
      else {
	lex_set_position (_l);
	lex_pop_position (_l);
	break;
      }
      if (!_mustbe (_tok_rpar)) return false;
    }
    else {
      lex_set_position (_l);
      lex_pop_position (_l);
      break;
    }
  } 

  return true;
}

#define ERR_RET lex_set_position (_l); lex_pop_position (_l); return false

bool SDF::_read_cell()
{
  lex_push_position (_l);

  if (lex_have (_l, _tok_lpar)) {
    if (!_mustbe (_CELL)) {
      ERR_RET;
    }
    if (!_mustbe (_tok_lpar)) {
      ERR_RET;
    }
    if (!_mustbe (_CELLTYPE)) {
      ERR_RET;
    }
    if (!_mustbe (l_string)) {
      ERR_RET;
    }
    A_NEW (_cells, struct sdf_cell);
    new (&A_NEXT (_cells)) sdf_cell();
    A_NEXT (_cells).celltype = Strdup (lex_prev (_l) + 1);
    A_NEXT (_cells).celltype[strlen (A_NEXT(_cells).celltype)-1] = '\0';
    if (!_mustbe (_tok_rpar)) {
      A_NEXT (_cells).clear();
      ERR_RET;
    }
    /* ( INSTANCE  ) */
    if (!_mustbe (_tok_lpar)) {
      A_NEXT (_cells).clear();
      ERR_RET;
    }
    
    if (!_mustbe (_INSTANCE)) {
      A_NEXT (_cells).clear();
      ERR_RET;
    }
  
    if (strcmp (lex_tokenstring (_l), "*") == 0) {
      // we're good!
      lex_getsym (_l);
    }
    else {
      // parse hierarchical id
      A_NEXT (_cells).inst = _parse_hier_id ();
      if (!A_NEXT (_cells).inst) {
	A_NEXT (_cells).clear();
	ERR_RET;
      }
    }

    if (!_mustbe (_tok_rpar)) {
      A_NEXT (_cells).clear();
      ERR_RET;
    }


    // we only parse DELAY annotations
    while (!lex_eof (_l) && !lex_have (_l, _tok_rpar)) {
      if (!_mustbe (_tok_lpar)) {
	A_NEXT (_cells).clear();
	ERR_RET;
      }
      if (lex_have (_l, _DELAY)) {
	if (lex_have (_l, _tok_lpar)) {
	  if (lex_sym (_l) == _ABSOLUTE || lex_sym (_l) == _INCREMENT) {
	    int type = (lex_sym (_l) == _ABSOLUTE) ? 0 : 1;

	    // delays are:
	    // IOPATH
	    // RETAIN
	    // COND, CONDELSE
	    // PORT
	    // INTERCONNECT
	    // NETDELAY
	    // DEVICE
	    
	    _skip_to_endpar ();
	  }
	  else if (lex_sym (_l) == _PATHPULSE
		   || lex_sym (_l) == _PATHPULSEPERCENT) {
	    // ignore these
	    _skip_to_endpar ();
	  }
	  else {
	    A_NEXT (_cells).clear();
	    ERR_RET;
	  }
	  if (!_mustbe (_tok_rpar)) {
	    A_NEXT (_cells).clear();
	    ERR_RET;
	  }
	}

	
	_skip_to_endpar ();
      }
      else if (lex_have (_l, _TIMINGCHECK) || lex_have (_l, _TIMINGENV)
	       || lex_have (_l, _LABEL)) {
	_skip_to_endpar ();
      }
      else {
	A_NEXT (_cells).clear();
	ERR_RET;
      }
      if (!_mustbe (_tok_rpar)) {
	A_NEXT (_cells).clear();
	ERR_RET;
      }
    }

    if (lex_eof (_l)) {
      A_NEXT (_cells).clear();
      ERR_RET;
    }
    else {
      A_INC (_cells);
      lex_pop_position (_l);
      return true;
    }
  }
  return false;
}



void SDF::Print (FILE *fp)
{
  fprintf (fp, "// Status: %s\n", _valid ? "valid" : "invalid");
  fprintf (fp, "(DELAYFILE\n");
#define EMIT_STRING(name,prefix)			\
  if (_h.prefix) {					\
    fprintf (fp, "  (%s \"%s\")\n", name, _h.prefix);	\
  }
  EMIT_STRING ("SDFVERSION", sdfversion);
  EMIT_STRING ("DESIGN", designname);
  EMIT_STRING ("DATE", date);
  EMIT_STRING ("VENDOR", vendor);
  EMIT_STRING ("PROGRAM", program);
  EMIT_STRING ("VERSION", version);
  fprintf (fp, "  (DIVIDER %c)\n", _h.divider);
  fprintf (fp, "  (VOLTAGE ");
  if (_h.voltage.issingleton()) {
    fprintf (fp, "%g)\n", _h.voltage.typ);
  }
  else {
    fprintf (fp, "%g:%g:%g)\n", _h.voltage.best, _h.voltage.typ,
	     _h.voltage.worst);
  }
  EMIT_STRING ("PROCESS", process);
  fprintf (fp, "  (TEMPERATURE ");
  if (_h.temp.issingleton()) {
    fprintf (fp, "%g)\n", _h.temp.typ);
  }
  else {
    fprintf (fp, "%g:%g:%g)\n", _h.temp.best, _h.temp.typ, _h.temp.worst);
  }
  fprintf (fp, "  (TIMESCALE ");
  const char *suffix;
  int ts;
  if (_h.timescale >= 1) {
    suffix = "s";
    ts = _h.timescale;
  }
  else if (_h.timescale >= 1e-3) {
    suffix = "ms";
    ts = 1e3*_h.timescale;
  }
  else if (_h.timescale >= 1e-6) {
    suffix = "us";
    ts = 1e6*_h.timescale;
  }
  else if (_h.timescale >= 1e-9) {
    suffix = "ns";
    ts = 1e9*_h.timescale;
  }
  else if (_h.timescale >= 1e-12) {
    suffix = "ps";
    ts = 1e12*_h.timescale;
  }
  else if (_h.timescale >= 1e-15) {
    suffix = "fs";
    ts = 1e15*_h.timescale;
  }
  else {
    fatal_error ("Internal inconsistency!");
  }
  fprintf (fp, "%d %s)\n", ts, suffix);

  for (int i=0; i < A_LEN (_cells); i++) {
    fprintf (fp, "  (CELL\n");
    if (_cells[i].celltype) {
      fprintf (fp, "    (CELLTYPE \"%s\")\n", _cells[i].celltype);
    }
    fprintf (fp, "    (INSTANCE ");
    if (_cells[i].inst) {
      _cells[i].inst->Print (fp, NULL, 0, _h.divider);
    }
    else {
      fprintf (fp, "*");
    }
    fprintf (fp, ")\n");
    fprintf (fp, "  )\n");
  }
  
  fprintf (fp, ")\n");
  return;
}


ActId *SDF::_parse_hier_id ()
{
  char *s = NULL;
  int sz = 128;
  int pos = 0;

  MALLOC (s, char, sz);

  lex_push_position (_l);

  while (!lex_eof (_l) && lex_sym (_l) != _tok_rpar) {
    int l = strlen (lex_tokenstring (_l));
    while (l + pos >= sz) {
      REALLOC (s, char, sz*2);
      sz *= 2;
    }
    strcat (s, lex_tokenstring (_l));
    pos += l;
    lex_getsym (_l);
    if (strlen (lex_whitespace (_l)) > 0) {
      break;
    }
  }
  if (pos == 0) {
    lex_set_position (_l);
    lex_pop_position (_l);
    return NULL;
  }

  ActId *ret = ActId::parseId (s, _h.divider, '[', ']', _h.divider);
  FREE (s);
  if (ret) {
    lex_pop_position (_l);
    return ret;
  }
  else {
    lex_set_position (_l);
    lex_pop_position (_l);
    fprintf (stderr, "Failed to parse hierarchical identifier!");
    return NULL;
  }
}


void SDF::_skip_to_endpar ()
{
  int count = 1;
  while (count != 0 && !lex_eof (_l)) {
    if (lex_sym (_l) == _tok_lpar) {
      count++;
    }
    else if (lex_sym (_l) == _tok_rpar) {
      count--;
    }
    if (count > 0) {
      lex_getsym (_l);
    }
  }
  return;
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

bool SDF::_read_delval (spef_triplet *f)
{
  spef_triplet dummy;
  
  if (!lex_have (_l, _tok_lpar)) {
    return false;
  }

  _err_ctxt = "parsing delval";

  if (!lex_have (_l, _tok_lpar)) {
    // rvalue only, already handled the (
    f->typ = -1000;
    Spef::getParasitics (_l, _tok_colon, f); // optional
    return _mustbe (_tok_rpar);
  }

  // parsed: ( (
  f->typ = -1000;
  Spef::getParasitics (_l, _tok_colon, f); // optional
  // delay
  if (!_mustbe (_tok_rpar)) {
    return false;
  }

  // parsed: ( rvalue

  if (!_mustbe (_tok_lpar)) {
    return false;
  }
  Spef::getParasitics (_l, _tok_colon, &dummy); // optional
  // r-limit and possibly e-limit
  if (!_mustbe (_tok_rpar)) {
    return false;
  }

  // parsed: ( rvalue rvalue
  if (lex_have (_l, _tok_lpar)) {
    Spef::getParasitics (_l, _tok_colon, &dummy); // optional
    // e-limit
    if (!_mustbe (_tok_rpar)) {
      return false;
    }
  }
  return _mustbe (_tok_rpar);
}

bool SDF::_read_delay (sdf_delay *s)
{
  int count = 0;

  if (lex_sym (_l) == _tok_lpar) {
    if (!_read_delval (&s->z2o)) {
      return false;
    }
  }
  else {
    _errmsg ("delay");
    return false;
  }
  if (lex_sym (_l) == _tok_lpar) {
    if (!_read_delval (&s->o2z)) {
      return false;
    }
  }
  else {
    s->o2z = s->z2o;
    return true;
  }
  count = 2;
  while (count < 12 && lex_sym (_l) == _tok_lpar) {
    spef_triplet dummy;
    if (!_read_delval (&dummy)) {
      return false;
    }
    count++;
  }
  return true;
}
