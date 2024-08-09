/*************************************************************************
 *
 *  Copyright (c) 2023-2024 Rajit Manohar
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
#include "sdf.h"

const char *sdf_path::_names[] =
  { "-none-",
    "IOPATH",
    "PORT",
    "INTERCONNECT",
    "DEVICE",
    "NETDELAY"
  };

/**
 * Initialize SDF reader 
 * @param mangled_ids is set to true if the IDs from the SDF came from
 * mangled characters generated by ACT
 */
SDF::SDF (bool mangled_ids)
{
  _l = NULL;
  _extended = false;
  _perinst = false;
  
#define TOKEN(a,b) a = -1;
#include "sdf.def"
  _valid = false;

  _last_error_report_line = -1;
  _last_error_report_col = -1;

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
  _h.energyscale = 1.0;

  _err_ctxt = NULL;

  _cellH = hash_new (4);
  //A_INIT (_cells);

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

  hash_bucket_t *b;
  hash_iter_t it;
  hash_iter_init (_cellH, &it);
  while ((b = hash_iter_next (_cellH, &it))) {
    struct sdf_celltype *ct = (struct sdf_celltype *) b->v;
    if (ct->all) {
      delete ct->all;
    }
    if (ct->inst) {
      chash_bucket_t *cb;
      chash_iter_t cit;
      chash_iter_init (ct->inst, &cit);
      while ((cb = chash_iter_next (ct->inst, &cit))) {
	sdf_cell *c = (sdf_cell *) cb->v;
	delete c;
      }
      chash_free (ct->inst);
    }
    FREE (ct);
  }
  hash_free (_cellH);
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
  char *s;

  if (lex_linenumber (_l) < _last_error_report_line ||
      (lex_linenumber (_l) == _last_error_report_line &&
       lex_colnumber (_l) < _last_error_report_col)) {
    return;
  }
  _last_error_report_line = lex_linenumber (_l);
  _last_error_report_col = lex_colnumber (_l);

  s = lex_errstring (_l);
  
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
  if (!_mustbe (_tok_lpar)) {
    _errmsg ("(");
    goto error;
  }

  // DELAYFILE
  if (!_mustbe (_DELAYFILE)) {
    if (!_mustbe (_XDELAYFILE)) {
      _errmsg ("DELAYFILE");
      goto error;
    }
    _extended = true;
  }
  else {
    _extended = false;
  }
  
  // sdf header
  if (!_read_sdfheader ()) goto error;

  while (_read_cell ()) {
    count++;
  }

  // )
  if (!_mustbe (_tok_rpar)) {
    _errmsg (")");
    goto error;
  }

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
  if (!_mustbe (_tok_lpar)) {
    _errmsg ("sdf-header");
    return false;
  }
  if (!_mustbe (_SDFVERSION)) {
    _errmsg ("SDFVERSION");
    return false;
  }
  if (!_mustbe (l_string)) {
    _errmsg ("version-string");
    return false;
  }

  if (_h.sdfversion) {
    FREE (_h.sdfversion);
  }
  _h.sdfversion = Strdup (lex_prev (_l)+1);
  _h.sdfversion[strlen (_h.sdfversion)-1] = '\0';

  if (!_mustbe (_tok_rpar)) {
    _errmsg (")");
    return false;
  }

#define PROCESS_STRING(field)			\
  do {						\
    lex_pop_position (_l);			\
    if (!_mustbe (l_string)) { _errmsg ("string"); return false; }	\
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
      else if (_extended && lex_have (_l, _ENERGYSCALE)) {
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
	  fprintf (stderr, "ENERGYSCALE specifier error!\n");
	  return false;
	}
	lex_getsym (_l);
	if (strcmp (lex_tokenstring (_l), "J") == 0) {
	  _h.energyscale = val;
	}
	else if (strcmp (lex_tokenstring (_l), "mJ") == 0) {
	  _h.energyscale = val*1e-3;
	}
	else if (strcmp (lex_tokenstring (_l), "uJ") == 0) {
	  _h.energyscale = val*1e-6;
	}
	else if (strcmp (lex_tokenstring (_l), "nJ") == 0) {
	  _h.energyscale = val*1e-9;
	}
	else if (strcmp (lex_tokenstring (_l), "pJ") == 0) {
	  _h.energyscale = val*1e-12;
	}
	else if (strcmp (lex_tokenstring (_l), "fJ") == 0) {
	  _h.energyscale = val*1e-15;
	}
	else if (strcmp (lex_tokenstring (_l), "aJ") == 0) {
	  _h.energyscale = val*1e-18;
	}
	else {
	  fprintf (stderr, "ENERGYSCALE specifier error!\n");
	}
	lex_getsym (_l);
      }
      else {
	lex_set_position (_l);
	lex_pop_position (_l);
	break;
      }
      if (!_mustbe (_tok_rpar)) {
	_errmsg (")");
	return false;
      }
    }
    else {
      lex_set_position (_l);
      lex_pop_position (_l);
      break;
    }
  } 

  return true;
}

#define ERR_RET if (cur) { cur->clear(); delete cur; } lex_set_position (_l); lex_pop_position (_l); return false

bool SDF::_read_cell()
{
  sdf_cell *cur = NULL;
  struct sdf_celltype *ct = NULL;
  ActId *instinfo = NULL;
  lex_push_position (_l);

  if (lex_have (_l, _tok_lpar)) {
    if (!_mustbe (_CELL)) {
      _errmsg ("CELL");
      ERR_RET;
    }
    if (!_mustbe (_tok_lpar)) {
      _errmsg ("(CELLTYPE");
      ERR_RET;
    }
    if (!_mustbe (_CELLTYPE)) {
      _errmsg ("CELLTYPE");
      ERR_RET;
    }
    if (!_mustbe (l_string)) {
      _errmsg ("string");
      ERR_RET;
    }
    // get sdf_celltype
    {
      hash_bucket_t *b;
      char *celltype = Strdup (lex_prev (_l)+1);
      celltype[strlen(celltype)-1] = '\0';
      b = hash_lookup (_cellH, celltype);
      if (!b) {
	b = hash_add (_cellH, celltype);
	NEW (ct, struct sdf_celltype);
	ct->all = NULL;
	ct->inst = NULL;
	ct->used = false;
	b->v = ct;
      }
      FREE (celltype);
      ct = (struct sdf_celltype *) b->v;
    }

    // allocate cell delay information
    cur = new sdf_cell;
    instinfo = NULL;

    if (!_mustbe (_tok_rpar)) {
      _errmsg (")");
      ERR_RET;
    }
    /* ( INSTANCE  ) */
    if (!_mustbe (_tok_lpar)) {
      _errmsg ("(INSTANCE");
      ERR_RET;
    }
    
    if (!_mustbe (_INSTANCE)) {
      _errmsg ("INSTANCE");
      ERR_RET;
    }
  
    if (strcmp (lex_tokenstring (_l), "*") == 0 || lex_sym (_l) == _tok_rpar) {
      // we're good!
      if (lex_sym (_l) == _tok_rpar) {
	/* nothing to do; implicit * */
      }
      else {
	lex_getsym (_l);
      }
    }
    else {
      // parse hierarchical id
      instinfo = _parse_hier_id ();
      if (!instinfo) {
	_errmsg ("path-to-inst");
	ERR_RET;
      }
      _perinst = true;
    }

    if (!_mustbe (_tok_rpar)) {
      _errmsg (")");
      ERR_RET;
    }

    // we only parse DELAY annotations
    while (!lex_eof (_l) && !lex_have (_l, _tok_rpar)) {
      if (!_mustbe (_tok_lpar)) {
	_errmsg ("(");
	ERR_RET;
      }
      if (lex_have (_l, _DELAY)) {
	if (lex_have (_l, _tok_lpar)) {
	  if (lex_sym (_l) == _ABSOLUTE || lex_sym (_l) == _INCREMENT) {
	    int type = (lex_sym (_l) == _ABSOLUTE) ? 1 : 0;
	    lex_getsym (_l);
	    
	    // delays are:
	    // IOPATH
	    // COND, CONDELSE
	    // PORT
	    // INTERCONNECT
	    // NETDELAY
	    // DEVICE

	    while (lex_have (_l, _tok_lpar)) {
	      sdf_path *p;
	      A_NEW (cur->_paths, sdf_path);
	      p = &A_NEXT (cur->_paths);
	      new (p) sdf_path();
	      p->abs = type;
	      if (lex_sym (_l) == _COND || lex_sym (_l) == _CONDELSE ||
		  lex_sym (_l) == _IOPATH) {
		if (lex_have (_l, _COND)) {
		  if (lex_have (_l, l_string)) {
		    // ignore the cond string label
		  }
		  sdf_cond_expr *e = _parse_expr ();
		  if (!e) {
		    ERR_RET;
		  }
		  p->e = e;
		  if (!_mustbe (_tok_lpar)) {
		    p->clear();
		    ERR_RET;
		  }
		}
		else if (lex_have (_l, _CONDELSE)) {
		  p->e = new sdf_cond_expr();
		  p->e->t = SDF_ELSE;
		  if (!_mustbe (_tok_lpar)) {
		    p->clear();
		    ERR_RET;
		  }
		}
		if (lex_have (_l, _IOPATH)) {
		  int have_edge = 0;
		  p->type = SDF_ELEM_IOPATH;
		  if (lex_sym (_l) == _tok_lpar) {
		    lex_getsym (_l);
		    if (lex_sym (_l) == _posedge) {
		      p->dirfrom = 1;
		    }
		    else if (lex_sym (_l) == _negedge) {
		      p->dirfrom = 2;
		    }
		    else {
		      p->clear();
		      _errmsg ("IOPATH expected posedge or negedge");
		      ERR_RET;
		    }
		    lex_getsym (_l);
		    have_edge = 1;
		  }
		  p->from = _parse_hier_id ();
		  if (!p->from) {
		    p->clear();
		    ERR_RET;
		  }
		  if (have_edge && lex_sym (_l) != _tok_rpar) {
		    p->clear ();
		    _errmsg ("IOPATH edge specifier error");
		    ERR_RET;
		  }
		  else if (have_edge) {
		    lex_getsym (_l);
		  }
		  p->to = _parse_hier_id ();
		  if (!p->to) {
		    p->clear ();
		    ERR_RET;
		  }
		  lex_push_position (_l);
		  if (lex_have (_l, _tok_lpar) && lex_have (_l, _RETAIN)) {
		    lex_pop_position (_l);
		    _skip_to_endpar();
		  }
		  lex_set_position (_l);
		  lex_pop_position (_l);
		  if (!_read_delay (&p->d)) {
		    p->clear ();
		    ERR_RET;
		  }
		  A_INC (cur->_paths);
		}
		else {
		  p->clear();
		  ERR_RET;
		}
		if (p->e) {
		  if (!_mustbe (_tok_rpar)) {
		    p->clear();
		    ERR_RET;
		  }
		}
	      }
	    else if (lex_have (_l, _PORT)) {
		p->type = SDF_ELEM_PORT;
		p->to = _parse_hier_id ();
		if (!p->to) {
		  p->clear ();
		  ERR_RET;
		}
		if (!_read_delay (&p->d)) {
		  p->clear ();
		  ERR_RET;
		}
		A_INC (cur->_paths);
	      }
	      else if (lex_have (_l, _INTERCONNECT)) {
		p->type = SDF_ELEM_INTERCONN;
		p->from = _parse_hier_id ();
		if (!p->from) {
		  p->clear();
		  ERR_RET;
		}
		p->to = _parse_hier_id ();
		if (!p->to) {
		  p->clear ();
		  ERR_RET;
		}
		if (!_read_delay (&p->d)) {
		  p->clear ();
		  ERR_RET;
		}
		A_INC (cur->_paths);
	      }
	      else if (lex_have (_l, _NETDELAY)) {
		p->type = SDF_ELEM_NETDELAY;
		p->to = _parse_hier_id ();
		if (!p->to) {
		  p->clear ();
		  ERR_RET;
		}
		if (!_read_delay (&p->d)) {
		  p->clear ();
		  ERR_RET;
		}
		A_INC (cur->_paths);
	      }
	      else if (lex_have (_l, _DEVICE)) {
		p->type = SDF_ELEM_DEVICE;
		p->to = _parse_hier_id ();
		if (!_read_delay (&p->d)) {
		  p->clear ();
		  ERR_RET;
		}
		A_INC (cur->_paths);
	      }
	      else {
		ERR_RET;
	      }
	      if (!_mustbe (_tok_rpar)) {
		p->clear ();
		ERR_RET;
	      }
	    }
	  }
	  else if (lex_sym (_l) == _PATHPULSE
		   || lex_sym (_l) == _PATHPULSEPERCENT) {
	    // ignore these
	    _skip_to_endpar ();
	  }
	  else {
	    ERR_RET;
	  }
	  if (!_mustbe (_tok_rpar)) {
	    ERR_RET;
	  }
	}
      }
      else if (_extended && lex_have (_l, _LEAKAGE)) {
	// XXX: extended syntax
	_skip_to_endpar ();
	if (!_mustbe (_tok_rpar)) {
	  ERR_RET;
	}
      }
      else if (_extended && lex_have (_l, _ENERGY)) {
	// XXX: extended syntax
	_skip_to_endpar ();
	if (!_mustbe (_tok_rpar)) {
	  ERR_RET;
	}
      }
      else if (lex_have (_l, _TIMINGCHECK) || lex_have (_l, _TIMINGENV)
	       || lex_have (_l, _LABEL)) {
	_skip_to_endpar ();
      }
      else {
	_errmsg ("delay/timing checks");
	ERR_RET;
      }
      if (!_mustbe (_tok_rpar)) {
	ERR_RET;
      }
    }

    if (lex_eof (_l)) {
      ERR_RET;
    }
    else {
      // ok now take this sdf_cell and put in into the hash table!
      if (instinfo) {
	if (!ct->inst) {
	  ct->inst = idhash_new (4);
	}
	chash_bucket_t *cb = chash_lookup (ct->inst, instinfo);
	if (!cb) {
	  cb = chash_add (ct->inst, instinfo);
	  cb->v = cur;
	}
	else {
	  warning ("Skipping inst-duplicates for now. FIX!");
	  delete cur;
	}
      }
      else {
	if (!ct->all) {
	  ct->all = cur;
	}
	else {
	  warning ("Skipping *-duplicates for now. FIX!");
	  delete cur;
	}
      }
      lex_pop_position (_l);
      return true;
    }
  }
  return false;
}



void SDF::Print (FILE *fp)
{
  fprintf (fp, "// Status: %s\n", _valid ? "valid" : "invalid");
  if (_extended) {
    fprintf (fp, "(XDELAYFILE\n");
  }
  else {
    fprintf (fp, "(DELAYFILE\n");
  }
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

  if (_extended) {
    fprintf (fp, "  (ENERGYSCALE ");
    const char *suffix;
    int ts;
    if (_h.energyscale >= 1) {
      suffix = "J";
      ts = _h.energyscale;
    }
    else if (_h.energyscale >= 1e-3) {
      suffix = "mJ";
      ts = 1e3*_h.energyscale;
    }
    else if (_h.energyscale >= 1e-6) {
      suffix = "uJ";
      ts = 1e6*_h.energyscale;
    }
    else if (_h.energyscale >= 1e-9) {
      suffix = "nJ";
      ts = 1e9*_h.energyscale;
    }
    else if (_h.energyscale >= 1e-12) {
      suffix = "pJ";
      ts = 1e12*_h.energyscale;
    }
    else if (_h.energyscale >= 1e-15) {
      suffix = "fJ";
      ts = 1e15*_h.energyscale;
    }
    else if (_h.energyscale >= 1e-18) {
      suffix = "aJ";
      ts = 1e15*_h.energyscale;
    }
    else {
      fatal_error ("Internal inconsistency!");
    }
    fprintf (fp, "%d %s)\n", ts, suffix);
  }

  hash_bucket_t *b;
  hash_iter_t it;
  hash_iter_init (_cellH, &it);
  while ((b = hash_iter_next (_cellH, &it))) {
    struct sdf_celltype *ct = (struct sdf_celltype *) b->v;
    if (ct->all) {
      fprintf (fp, "  (CELL\n");
      fprintf (fp, "    (CELLTYPE \"%s\")\n", b->key);
      fprintf (fp, "    (INSTANCE *)\n");
      ct->all->Print (fp, "    ", _h.divider);
    }
    if (ct->inst) {
      chash_bucket_t *cb;
      chash_iter_t cit;
      chash_iter_init (ct->inst, &cit);
      while ((cb = chash_iter_next (ct->inst, &cit))) {
	sdf_cell *c = (sdf_cell *) cb->v;
	ActId *inst = (ActId *) cb->key;
	fprintf (fp, "  (CELL\n");
	fprintf (fp, "    (CELLTYPE \"%s\")\n", b->key);
	fprintf (fp, "    (INSTANCE ");
	inst->Print (fp, NULL, 0, _h.divider);
	fprintf (fp, ")\n");
	c->Print (fp, "    ", _h.divider);
      }
    }
  }
  fprintf (fp, ")\n");
  return;
}

static int _valid_escaped_chars (char c)
{
  if (c == '!' || c == '"' || c == '#' || c == '$' || c == '%' || c == '&' ||
      c == '(' || c == ')' || c == '*' || c == '+' || c == ',' || c == '-' ||
      c == '.' || c == '/' || c == ':' ||
      c == ';' || c == '<' || c == '=' || c == '>' || c == '?' || c == '@' ||
      c == '[' || c == '\\' || c == ']' ||c == '^' || c == '`' || c == '{' ||
      c == '|' || c == '}' || c == '~') {
    return 1;
  }
  return 0;
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
    for (int i=0; lex_tokenstring(_l)[i]; i++) {
      if (lex_tokenstring (_l)[i] == '\\') {
	i++;
      }
      s[pos++] = lex_tokenstring (_l)[i];
    }
    s[pos] = '\0';
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

  if (_a) {
    char *tmp;
    MALLOC (tmp, char, sz);
    _a->unmangle_string (s, tmp, sz);
    FREE (s);
    s = tmp;
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


// delval-list 
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






/*------------------------------------------------------------------------
 *
 * Conditional expression parser
 *
 *------------------------------------------------------------------------
 */

sdf_cond_expr *SDF::_parse_base ()
{
  sdf_cond_expr *e;
  ActId *id;
  if (lex_have (_l, _tok_lpar)) {
    e = _parse_expr ();
    if (!e) {
      return NULL;
    }
    if (!_mustbe (_tok_rpar)) {
      delete e;
      return NULL;
    }
  }
  else if (lex_have (_l, _tok_const0) || lex_have (_l, _tok_const0n)) {
    e = new sdf_cond_expr ();
    e->t = SDF_FALSE;
  }
  else if (lex_have (_l, _tok_const1) || lex_have (_l, _tok_const1n)) {
    e = new sdf_cond_expr ();
    e->t = SDF_TRUE;
  }
  else if (lex_have (_l, _tok_not) || lex_have (_l, _tok_not2)) {
    sdf_cond_expr *t = _parse_expr ();
    if (!t) {
      return NULL;
    }
    e = new sdf_cond_expr ();
    e->t = SDF_NOT;
    e->l = t;
  }
  else if ((id = _parse_hier_id ())) {
    e = new sdf_cond_expr ();
    e->t = SDF_VAR;
    e->l = (sdf_cond_expr *) id;
  }
  else {
    return NULL;
  }
  return e;
}

/*  ==, != */
sdf_cond_expr *SDF::_parse_expr_1 ()
{
  sdf_cond_expr *t1, *t2, *e;
  int type;

  t1 = _parse_base();
  if (!t1) {
    return NULL;
  }
  type = (lex_sym (_l) == _tok_eq ? 0 : 1);
  if (lex_have (_l, _tok_eq) || lex_have (_l, _tok_ne)) {
    t2 = _parse_base();
    if (!t2) {
      delete t1;
      return NULL;
    }
  }
  else {
    return t1;
  }
  e = new sdf_cond_expr ((type == 0 ? SDF_EQ : SDF_NE), t1, t2);
  return e;
}

#define ASSOC_OPERATOR(name, nextlevel, type, sym)	\
sdf_cond_expr *SDF::name ()				\
{							\
  sdf_cond_expr *ret = NULL;				\
  sdf_cond_expr *t;					\
							\
  do {							\
    t = nextlevel ();					\
    if (!t) return ret;					\
    if (!ret) { ret = t; }				\
    else { ret = new sdf_cond_expr (type, ret, t); }	\
  } while (lex_have (_l, sym));				\
  return ret;						\
}

/* & */ ASSOC_OPERATOR(_parse_expr_2, _parse_expr_1, SDF_AND, _tok_and)
/* ^ */ ASSOC_OPERATOR(_parse_expr_3, _parse_expr_2, SDF_XOR, _tok_xor)
/* | */ ASSOC_OPERATOR(_parse_expr_4, _parse_expr_3, SDF_OR, _tok_or)
/* && */ ASSOC_OPERATOR(_parse_expr_5, _parse_expr_4, SDF_AND, _tok_andand)
/* || */ ASSOC_OPERATOR(_parse_expr, _parse_expr_5, SDF_OR, _tok_oror)


void sdf_cell::Print (FILE *fp, const char *ts, char divider)
{
  fprintf (fp, "%s(DELAY\n", ts);
  int prev = -1;
  for (int j=0; j < A_LEN (_paths); j++) {
    sdf_path *p = &_paths[j];
    if (p->abs != prev) {
      if (prev != -1) {
	fprintf (fp, "%s)\n", ts);
      }
      prev = p->abs;
      if (prev) {
	fprintf (fp, "%s (ABSOLUTE\n", ts);
      }
      else {
	fprintf (fp, "%s (INCREMENT\n", ts);
      }
    }
    fprintf (fp, "%s  ", ts);
    p->Print (fp, divider);
    fprintf (fp, "\n");
  }
  if (prev != -1) {
    fprintf (fp, "%s )\n", ts);
  }

  if (A_LEN (_epaths) > 0) {
    fprintf (fp, "%s(ENERGY\n", ts);
    prev = -1;
    for (int j=0; j < A_LEN (_epaths); j++) {
      sdf_path *p = &_epaths[j];
      if (p->abs != prev) {
	if (prev != -1) {
	  fprintf (fp, "%s)\n", ts);
	}
	prev = p->abs;
	if (prev) {
	  fprintf (fp, "%s (ABSOLUTE\n", ts);
	}
	else {
	  fprintf (fp, "%s (INCREMENT\n", ts);
	}
      }
      fprintf (fp, "%s  ", ts);
      p->Print (fp, divider);
      fprintf (fp, "\n");
    }
    if (prev != -1) {
      fprintf (fp, "%s )\n", ts);
    }
  }
  fprintf (fp, "%s)\n", ts);
}  


sdf_celltype *SDF::getCell (const char *s)
{
  if (!s) return NULL;
  hash_bucket_t *b;
  b = hash_lookup (_cellH, s);
  if (!b) {
    return NULL;
  }
  return (sdf_celltype *) b->v;
}

sdf_cell *sdf_celltype::getInst (ActId *id)
{
  if (!id) {
    return all;
  }
  chash_bucket_t *cb;
  cb = chash_lookup (inst, id);
  if (cb) {
    return (sdf_cell *) cb->v;
  }
  else {
    return all;
  }
}


void SDF::reportUnusedCells (const char *msg, FILE *fp)
{
  hash_bucket_t *b;
  hash_iter_t it;
  hash_iter_init (_cellH, &it);
  while ((b = hash_iter_next (_cellH, &it))) {
    sdf_celltype *ci = (sdf_celltype *) b->v;
    if (!ci->used) {
      fprintf (fp, "%s: %s was not used.\n", msg, b->key);
    }
  }
}
