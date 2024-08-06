/*************************************************************************
 *
 *  This is the SPEF merging pass
 *
 *  Copyright (c) 2022-2024 Rajit Manohar
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
#ifndef __ACT_ANNOTATE_PASS_H__
#define __ACT_ANNOTATE_PASS_H__


/**
 * 
 * @file annotate_pass.h
 * @brief ACT pass that reads and merges in SPEF with netlist
 *
 */


extern "C" {

  void annotate_pass_init (ActPass *ap);
  void annotate_pass_run (ActPass *ap, Process *p);
  void annotate_pass_recursive (ActPass *ap, Process *p, int mode);
  void *annotate_pass_proc (ActPass *ap, Process *p, int mode);
  void *annotate_pass_data (ActPass *ap, Data *d, int mode);
  void *annotate_pass_chan (ActPass *ap, Channel *c, int mode);
  int annotate_pass_runcmd (ActPass *ap, const char *name);
  void annotate_pass_free (ActPass *ap, void *v);
  void annotate_pass_done (ActPass *ap);

}

#endif /* __ACT_ANNOTATE_PASS_H__ */
