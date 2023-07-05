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
#ifndef __ACT_SDF_H__
#define __ACT_SDF_H__

#include <common/lex.h>
#include <common/hash.h>
#include <common/array.h>
#include <act/act.h>
#include <act/spef.h>

/**
 *
 * @file sdf.h
 * @brief SDF reader/writer API
 *
 */

class SDF;


/*
 *
 * SDF permits up to six different delay specs. These delays specify
 *
 *  0->1, 1->0, 0->Z, Z->1, 1->Z, Z->0
 *
 * We currently only use 0->1 and 1->0.
 *
 * Im addition, pulse widths e-limit and r-limit can be specified.
 *   
 * The r-limit (rejection limit) says that a pulse has to be at least
 * a minimum width before it propagates. Any narrower pulse is
 * filtered.
 *
 * The e-limit (error limit), which should be greater than the r-limit
 * to be meaningful, says that any pulse narrower than the e-limit
 * (but at least the r-limit) will cause the output to become X,
 * rather than propagate.
 *
 */

struct sdf_delay {
  spef_triplet z2o;		///< zero-to-one delay
  spef_triplet o2z;		///< one-to-zero delay

  sdf_delay() { init(); }
  
  void init() {
    z2o.best = 0;
    z2o.typ = 0;
    z2o.worst = 0;
    o2z = z2o;
  }

  void Print (FILE *fp) {
    if (z2o.best == z2o.typ && z2o.best == z2o.worst) {
      fprintf (fp, "(%g) ", z2o.best);
    }
    else {
      fprintf (fp, "(%g:%g:%g) ", z2o.best, z2o.typ, z2o.worst);
    }
    
    if (o2z.best == o2z.typ && o2z.best == o2z.worst) {
      fprintf (fp, "(%g)", o2z.best);
    }
    else {
      fprintf (fp, "(%g:%g:%g)", o2z.best, o2z.typ, o2z.worst);
    }
  }
};

enum sdf_cond_expr_type {
  SDF_AND, SDF_OR, SDF_NOT, SDF_VAR,
  SDF_XOR, SDF_EQ, SDF_NE,
  SDF_TRUE, SDF_FALSE, SDF_BAD,
  SDF_ELSE
};

struct sdf_cond_expr {
  sdf_cond_expr_type t;
  sdf_cond_expr *l, *r;
  sdf_cond_expr() { t = SDF_BAD; l = NULL; r = NULL; }
  sdf_cond_expr(sdf_cond_expr_type _t, sdf_cond_expr *x, sdf_cond_expr *y) {
    t = _t; l = x; r = y;
  }

  bool isElse() {
    return t == SDF_ELSE ? true : false;
  }

  void Print (FILE *fp, char delim) {
    switch (t) {
    case SDF_TRUE:
      fprintf (fp, "1'b0");
      break;
    case SDF_FALSE:
      fprintf (fp, "1'b1");
      break;
    case SDF_VAR:
      ((ActId *)l)->Print (fp, NULL, 0, delim);
      break;
    case SDF_NOT:
      fprintf (fp, "~");
      l->Print (fp, delim);
      break;
    case SDF_AND:
      l->Print (fp, delim);
      fprintf (fp, " & ");
      r->Print (fp, delim);
      break;
    case SDF_OR:
      l->Print (fp, delim);
      fprintf (fp, " | ");
      r->Print (fp, delim);
      break;
    case SDF_XOR:
      l->Print (fp, delim);
      fprintf (fp, " ^ ");
      r->Print (fp, delim);
      break;
    case SDF_EQ:
      l->Print (fp, delim);
      fprintf (fp, " == ");
      r->Print (fp, delim);
      break;
    case SDF_NE:
      l->Print (fp, delim);
      fprintf (fp, " != ");
      r->Print (fp, delim);
      break;
    default:
      fatal_error ("BAD condition!");
      break;
    }
  }
  
  ~sdf_cond_expr() {
    if (t == SDF_BAD) {
      return;
    }
    if (t == SDF_VAR) {
      delete ((ActId *)l);
    }
    else if (t == SDF_NOT) {
      delete l;
    }
    else if (t == SDF_TRUE || t == SDF_FALSE) {
      // nothing
    }
    else {
      delete l;
      delete r;
    }
  }
};


#define SDF_ELEM_NONE        0

#define SDF_ELEM_IOPATH      1
   /**< IOPATH path delay. This goes from an input pin to an output
        pin of a device. It can have a condition specified as well **/

#define SDF_ELEM_PORT        2
   /**< PORT delay: input delay to an input port of a device */

#define SDF_ELEM_INTERCONN   3
   /**< INTERCONNECT delay: from driver to another input pin */

#define SDF_ELEM_DEVICE      4
   /**< DEVICE delay: with an optional output port, this is the input
        to output delay for the device */

#define SDF_ELEM_NETDELAY    5
  /**< NETDELAY delay: used to have a single delay for a net, from all
       drivers to all input pins */

struct sdf_path {
  static const char *_names[];
  
  unsigned int type:3;		///< 1 = iopath, 2 = port, 3 =
				///< interconnect, 4 = device, 5 =
				///< netdelay
  unsigned int abs:1;		//<  1 for ABSOLUTE, 2 for INCREMENT

  
  sdf_cond_expr *e;		///< conditional expression, if any
  ActId *from;			///< source
  ActId *to;			///< target
  sdf_delay d;

  void Print (FILE *fp, char delim) {
    if (e) {
      fprintf (fp, "(COND");
      if (e->isElse()) {
	fprintf (fp, "ELSE ");
      }
      else {
	fprintf (fp, " ");
	e->Print (fp, delim);
	fprintf (fp, " ");
      }
    }
    fprintf (fp, "(%s ", _names[type]);
    if (from) {
      from->Print (fp, NULL, 0, delim);
      fprintf (fp, " ");
    }
    if (to) {
      to->Print (fp, NULL, 0, delim);
      fprintf (fp, " ");
    }
    d.Print (fp);
    fprintf (fp, ")");
    if (e) {
      fprintf (fp, " )");
    }
  }

  sdf_path() {
    type = SDF_ELEM_NONE;
    e = NULL;
    from = NULL;
    to = NULL;
    d.init();
  }
  ~sdf_path() { clear (); }
  void clear() {
    if (e) {
      delete e;
    }
    if (from) {
      delete from;
    }
    if (to) {
      delete to;
    }
  }

};


struct sdf_cell {
  char *celltype;		///< This is the type name, but it can
				///< also be a hierarchy path (!)
  
  ActId *inst;			///< NULL if this is *, otherwise it
				///< is a hierarchy path
  
  // delay record
  A_DECL (sdf_path, _paths);

  sdf_cell() {
    celltype = NULL;
    inst = NULL;
    A_INIT (_paths);
  }
  ~sdf_cell() {
    clear();
  }
  void clear() {
    if (celltype) FREE(celltype);
    celltype = NULL;
    if (inst) delete inst;
    inst = NULL;
    for (int i=0; i < A_LEN (_paths); i++) {
      _paths[i].~sdf_path();
    }
    A_FREE (_paths);
  }
};



class SDF {
 public:
  SDF (bool mangled_ids = false);
  ~SDF ();

  /**
   * Read in a SPEF file
   * @param fp is the FILE pointer to the file
   * @return true if read was successful, false otherwise
   */
  bool Read (FILE *fp);

  /**
   * Read in a SPEF file
   * @param name is the file name
   * @return true if read was successful, false otherwise
   */
  bool Read (const char *name);

  /**
   * Print the SDF data structure 
   * @param fp is the output file
   */
  void Print (FILE *fp);
  

 private:
  Act *_a;			///< ACT data structure, if any
  

  struct sdf_header {
    char *sdfversion;
    char *designname;
    char *date;
    char *vendor;
    char *program;
    char *version;
    char divider;
    spef_triplet voltage;
    char *process;
    spef_triplet temp;
    float timescale;
  } _h;

  A_DECL (struct sdf_cell, _cells);


  bool _valid;			///< true if a read succeeded and a
				///< valid SDF file was read in

  /*-- parser state --*/
  LEX_T *_l;		       ///< internal lexer state
  int
#define TOKEN(a,b) a,
#include <act/sdf.def>
    _ENDTOKEN;
  bool _read_sdfheader ();
  bool _mustbe (int tok);
  bool _read_cell ();
  void _skip_to_endpar ();
  ActId *_parse_hier_id ();
  void _errmsg (const char *buf);
  bool _read_delval (spef_triplet *f);
  bool _read_delay (sdf_delay *d);
  sdf_cond_expr *_parse_base ();
  sdf_cond_expr *_parse_expr_1 ();
  sdf_cond_expr *_parse_expr_2 ();
  sdf_cond_expr *_parse_expr_3 ();
  sdf_cond_expr *_parse_expr_4 ();
  sdf_cond_expr *_parse_expr_5 ();
  sdf_cond_expr *_parse_expr ();
  
  const char *_err_ctxt;
  int _last_error_report_line, _last_error_report_col;

  // actual delay records
};



#endif /* __ACT_SPEF_H__ */

