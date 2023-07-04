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
};

struct sdf_cell {
  char *celltype;		///< This is the type name, but it can
				///< also be a hierarchy path (!)
  
  ActId *inst;			///< NULL if this is *, otherwise it
				///< is a hierarchy path
  
  // delay record

  sdf_cell() { celltype = NULL; inst = NULL; }
  void clear() { if (celltype) FREE(celltype); if (inst) delete inst; }
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
#include "sdf.def"
    _ENDTOKEN;
  bool _read_sdfheader ();
  bool _mustbe (int tok);
  bool _read_cell ();
  void _skip_to_endpar ();
  ActId *_parse_hier_id ();
  void _errmsg (const char *buf);
  bool _read_delval (spef_triplet *f);
  bool _read_delay (sdf_delay *d);
  const char *_err_ctxt;

};



#endif /* __ACT_SPEF_H__ */
