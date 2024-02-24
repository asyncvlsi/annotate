/*************************************************************************
 *
 *  This is the SPEF merging pass
 *
 *  Copyright (c) 2022 Rajit Manohar
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
