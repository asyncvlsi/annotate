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
#ifndef __ACT_SPEF_H__
#define __ACT_SPEF_H__

#include <common/lex.h>
#include <common/hash.h>
#include <common/array.h>
#include <act/act.h>

struct spef_triplet {
  float best, typ, worst;
};

struct spef_attributes {
  unsigned int simple:1;	// 1 if simple attribute (not used)
  unsigned int coord:1;		// coordinates specified
  unsigned int load:1;		// load specified
  unsigned int slew:1;		// slew specified
  unsigned int slewth:1;	// threshold specified as part of slew
  unsigned int drive:1;		// driving cell

  double cx, cy;		// coordinates
  spef_triplet l;		// load

  spef_triplet s1, s2;		// slew
  spef_triplet t1, t2;		// thresholds

  ActId *cell;			// cell specified
};

struct spef_ports {
  ActId *inst, *port;
  int dir;			// 0 = in, 1 = out, 2 = bidir
  spef_attributes *a;
};

struct spef_defines {
  int phys:1;			// PDEFINE rather than DEFINE
  ActId *inst;			// instance name
  char *qstring;
};

struct spef_detailed_net {
  /* connections */
  /* caps */
  /* resistances */
  /* inductances */
  int x;
};

struct spef_reduced_net {
  /* list of driver, cell, pi model, load description */
  int x;
};

struct spef_net {
  ActId *net;			// net name
  spef_triplet tot_cap;		// total cap
  int routing_confidence;	// confidence
  int type;			// 0 = D_NET, 1 = R_NET
  union {
    spef_detailed_net d;
    spef_reduced_net r;
  } u;
};

class Spef {
 public:
  Spef();
  ~Spef();

  /* returns true on success, false on error */
  bool Read (FILE *fp);
  
 private:
  LEX_T *_l;

  /* tokens */
  int
#define TOKEN(a,b) a,
#include "spef.def"    

    _tok_hier_delim,
    _tok_pin_delim,
    _tok_prefix_bus_delim,
    _tok_suffix_bus_delim;
    
  char *_prevString ();
  char *_getTokId();
  
  ActId *_getTokPhysicalRef ();
  ActId *_getTokPath (); // lsb is set to 1 if it is an abs path
  ActId *_getTokName();
  
  ActId *_getIndex();	    // return ID from index, if it is an index

  // return true on success, false otherwise
  // isphy = true for physical ports, false otherwise
  // returns inst name and port name
  bool _getPortName (bool isphy, ActId **inst_name, ActId **port);

  bool _getParasitics (spef_triplet *t);
  spef_attributes *_getAttributes();

  /* read each section */
  bool _read_header ();
  bool _read_units ();
  bool _read_name_map ();
  bool _read_power_def ();
  bool _read_external_def ();
  bool _read_define_def ();
  bool _read_variation_def ();
  bool _read_internal_def ();

  /* data from the SPEF */
  char *_spef_version;
  char *_design_name;
  char *_date;
  char *_vendor;
  char *_program;
  char *_version;
  double _time_unit;
  double _c_unit;
  double _r_unit;
  double _l_unit;

  /* name map */
  struct iHashtable *_nH;

  A_DECL (ActId *, _power_nets);
  A_DECL (ActId *, _gnd_nets);

  A_DECL (spef_ports, _ports);
  A_DECL (spef_ports, _phyports);

  A_DECL (spef_defines, _defines);
};



#endif /* __ACT_SPEF_H__ */
