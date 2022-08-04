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


/**
 *
 * @file spef.h
 * @brief SPEF reader/writer API
 *
 */

/**
 * Use this macro to access any of the ActId pointers in the Spef data
 * structures.
 */
#define SPEF_GET_PTR(x)  ((ActId *)(((unsigned long)(x))&~3UL))

/**
 * Returns 1 if the ActId pointer is in fact an absolute path to an
 * identifier specified in the Spef rather than a simple path.
 */
#define SPEF_IS_ABS(x) (((unsigned long)x) & 1)

class Spef; 

/** SPEF triplet structure for values. Values correspond to three
 * different operating points: typical, best-case, and worst-case.
 * Values are used for any paramter in the SPEF file.
 */
struct spef_triplet {
  /// best case value
  float best;

  /// typical value
  float typ;

  /// worst-case value
  float worst;
};


/** A collection of SPEF attributes that can be associated with a
 *  number of different parts of a SPEF file. The structure has flags
 *  to determine which attributes were in fact found in the file, and
 *  it also contains the values of the attributes that were found.
 */
struct spef_attributes {
  
  /// 1 if simple attribute (not used)
  unsigned int simple:1;
  
  /// coordinates are specified
  unsigned int coord:1;
  
  /// load is specified
  unsigned int load:1;

  /// slew is specified
  unsigned int slew:1;

  /// threshold is specified as part of slew
  unsigned int slewth:1;

  /// driving cell is specified
  unsigned int drive:1;

  /// x-coordinate, if coordinates specified
  double cx;
  
  /// y-coordinate, if coordinates specified
  double cy;

  /// value of load capacitance, if specified
  spef_triplet l;

  /// the rising slew, if specified
  spef_triplet s1;

  /// the falling slew, if specified
  spef_triplet s2;

  /// the rising threshold (%), if specified
  spef_triplet t1;

  /// the falling threshold (%), if specified
  spef_triplet t2;

  /// driving cell type
  ActId *cell;
};


/** An individual SPEF port. This specifies the instance + port
 * combination, along with any attributes for the port and direction
 * specifications.
 */
struct spef_ports {
  /// The instance ID
  ActId *inst;

  /// The port from this instance
  ActId *port;

  /// SPEF attributes associated with this port
  spef_attributes *a;

  /// Port direction, 0 = in, 1 = out, 2 = bidir
  unsigned int dir;
};


/** SPEF definitions. These are used for hierarchical SPEF files. The
 * actual SPEF file corresponding to the definition is supposed to be
 * determined in some "out-of-band" fashion. Parasitics from the
 * sub-spef files are supposed to be merged in with the parent.
 */
struct spef_defines {
  /// 1 for PDEFINE, 0 for DEFINE
  unsigned int phys:1;

  /// name of the instance
  ActId *inst;

  /// design name, which must match the design name in the child SPEF
  char *design_name;

  /// Spef data structure for the sub-SPEF corresponding to this definition
  Spef *spef;
};


/** An individual end-point of a SPEF connection. A detailed net
 * description consists of a number of these connections. A connection
 * of type "P" is a port. A connection of type "I" is an internal
 * node. The type "N" is used to specify coordinates for
 * an internal node.
 */
struct spef_conn {
  /// Type of connection: 0 = *P, 1 = *I, 2 = *N
  unsigned int type:2;

  /// Direction flag: 0 = in, 1 = out, 2 = bidir
  unsigned int dir:2;

  /// Connection name (path to the instance)
  ActId *inst;

  /// Pin name
  ActId *pin;

  /// Any attributes associate with this connection end-point
  spef_attributes *a;

  /// node number used to sub-divide net into different physical points for *N
  int ipin;

  /// coordinates for *N
  float cx, cy;
};



/**
 * A single SPEF node. It consists of an instance:pin, and an optional
 * index that specifies the piece of the electrical node this
 * corresponds to.
 *
 * The instance name can be NULL. The integer for internal nodes can
 * also be omitted, in which case the idx field will be -1.
 */
struct spef_node {
  /// instance name, if any; could be NULL
  ActId *inst;

  /// pin name
  ActId *pin;			// or pin

  /// optional integer for internal nodes (-1 if omitted)
  int idx;
};


/**
 * This is used to represent a parasitic value. For capacitors, the
 * second node can be NULL (for cap to ground), or non-NULL for
 * coupling capacitances. For resistors and inductors, both nodes must
 * be non-NULL.
 */
struct spef_parasitic {
  /// The integer id for this particular parasitic value
  int id;

  /// The first node associated with the parasitic value.
  spef_node n;

  /// The second node associated with the parastic value.
  spef_node n2;

  /// The actual value
  spef_triplet val;
  
  /* XXX: sensitivity: use with variations */
};


/**
 * This holds the information for a SPEF *D_NET detailed net
 * specification. It contains a list of connection end-points,
 * followed by capacitors, resistors, and inductors for the net.
 */
struct spef_detailed_net {
  /// array of connections for the detailed net
  A_DECL (spef_conn, conn);

  /// array of capacitors for the detailed net
  A_DECL (spef_parasitic, caps);

  /// array of resistors for the detailed net
  A_DECL (spef_parasitic, res);

  /// array of inductors for the detailed net
  A_DECL (spef_parasitic, induc);
};


/**
 * This holds a term for the RC reduced model for a net. The instance
 * and pin name is included, followed by the RC value and optionally
 * the complex pole and residue values for the particular node.
 */
struct spef_rc_desc {
  /// instance name
  ActId *inst;

  /// pin name
  ActId *pin;

  /// RC value
  spef_triplet val;

  /**
   * Pole/residue value structure
   */
  struct pole_desc {
    /// numerical index value, -1 means not specified
    int idx;

    /// real-part of value
    spef_triplet re;

    /// imaginary part of value
    spef_triplet im;
  } pole /** value of pole */, residue /** value of residue */;
};


/**
 * Data structure that holds the information for one driver in a
 * reduced *R_NET model for a net.
 */
struct spef_reduced {
  /// instance name for driver
  ActId *driver_inst;

  /// pin for driver
  ActId *pin;

  /// cell type for driving cell
  ActId *cell_type;

  spef_triplet c2 /** pi-model C2 capacitance, the cap closest to the
		      driving cell */,
    r1 /** pi model resistance */, c1 /** pi-model C1 capacitance,
					  the cap furthest from the
					  driving cell */;

  /// array of RC values for different end-points of the net
  A_DECL (spef_rc_desc, rc);
};


/**
 * A spef *R_NET consists of a collection of drivers, and this holds
 * the information for all the drivers.
 */
struct spef_reduced_net {
  /// an array of drivers, each with their own end-point RC values and parasitics
  A_DECL (spef_reduced, drivers);
};

  
/**
 * A SPEF net with parasitic information
 */
struct spef_net {
  /// the net name
  ActId *net;

  /// total capacitance for the net
  spef_triplet tot_cap;

  /// routing confidence integer
  int routing_confidence;

  /// 0 = D_NET, 1 = R_NET, 2 = D_PNET, 3 = R_PNET
  int type;
  
  union {
    /// if this is a detailed net, the information is here
    spef_detailed_net d;

    /// if this is a reduced net spec, the information is here
    spef_reduced_net r;
  } u /** the parasitic information */;
};


/**
 *  API to read/write/query a SPEF file
 */
class Spef {
 public:

  /// @param mangled_ids should be set to true if the SPEF file
  /// corresonds to layout where the names were generated using ACT
  /// name mangling conventions. Doing so will convert all the names
  /// back into sane ACT names.
  Spef(bool mangled_ids = false);
  ~Spef();

  /* returns true on success, false on error */
  bool Read (FILE *fp);

  /* print */
  void Print (FILE *fp);

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


  bool _getPinPortInternal (spef_node *n);

  bool _getParasitics (spef_triplet *t);
  bool _getComplexParasitics (spef_triplet *re, spef_triplet *im);
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

  Act *_a;

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

  char _divider, _delimiter;
  char _bus_prefix_delim, _bus_suffix_delim;

  /* name map */
  struct iHashtable *_nH;

  unsigned int _valid:1;	// valid spef!

  A_DECL (ActId *, _power_nets);
  A_DECL (ActId *, _gnd_nets);

  A_DECL (spef_ports, _ports);
  A_DECL (spef_ports, _phyports);

  A_DECL (spef_defines, _defines);
  A_DECL (spef_net, _nets);
};



#endif /* __ACT_SPEF_H__ */
