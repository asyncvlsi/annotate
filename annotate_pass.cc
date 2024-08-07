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
#include <act/act.h>
#include <act/passes.h>
#include "spef.h"
#include "sdf.h"
#include "annotate_pass.h"

void annotate_pass_init (ActPass *ap)
{
  Act *a = ap->getAct();
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *>(ap);
  Assert (dp, "Hmm..");

  ActPass *bp = a->pass_find ("booleanize");

  if (!bp) {
    bp = new ActBooleanizePass (a);
  }
  dp->addDependency ("booleanize");
}


static Spef *load_spef (Process *p)
{
  Spef *spf;
  char buf[1024];
  char buf2[1024];
  char *ns = NULL;
  FILE *fp;
    
  Assert(p, "What?");
  if (p->getns() && p->getns() != ActNamespace::Global()) {
    ns = p->getns()->Name();
  }
  if (ns) {
    snprintf (buf, 1024, "spef.%s::%s", ns, p->getName());
    snprintf (buf2, 1024, "%s::%s.spef", ns, p->getName());
  }
  else {
    snprintf (buf, 1024, "spef.%s", p->getName());
    snprintf (buf2, 1024, "%s.spef", p->getName());
  }
  if (ns) {
    FREE (ns);
  }
  spf = new Spef (true);
  if (config_exists (buf)) {
    fp = fopen (config_get_string (buf), "r");
    if (!fp) {
      warning ("Could not open SPEF file `%s' for reading",
	       config_get_string (buf));
      delete spf;
      return NULL;
    }
  }
  else {
    // look for <process>.spef
    fp = fopen (buf2, "r");
    if (!fp) {
      delete spf;
      return NULL;
    }
  }
  spf->Read (fp);
  fclose (fp);
  return spf;
}

/* Not defining this
void annotate_pass_run (ActPass *ap, Process *p)
void annotate_pass_recursive (ActPass *ap, Process *p, int mode);
*/

void *annotate_pass_proc (ActPass *ap, Process *p, int mode)
{
  char *s = NULL;
  Spef *spf = NULL;
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *> (ap);

  if (!p) {
    /* run on the top level global namespace */
    printf ("Annotation pass must be run with a specified top-level process only.");
    return NULL;
  }
  if (!dp->getRoot()) {
    return NULL;
  }

  spf = load_spef (p);
  if (spf->isValid()) {
    return spf;
  }
  else {
    delete spf;
    return NULL;
  }
}

void annotate_pass_free (ActPass *ap, void *v)
{
  Spef *spf = (Spef *) v;
  if (spf) {
    delete spf;
  }
}


void annotate_pass_done (ActPass *ap)
{
  // nothing to do here!
}

int annotate_pass_runcmd (ActPass *ap, const char *name)
{
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *> (ap);
  Assert (dp, "What?");
  if (!name) return 0;
  if (strcmp (name, "split-net") == 0) {
     Process *p = (Process *) dp->getPtrParam ("proc");
     const char *net =  (const char *) dp->getPtrParam  ("net");
     Assert (p && net, "What?");
     Spef *spf = (Spef *) dp->getMap (p);
     if (!spf) { return 0; }

     if (spf->isSplit (net)) {
       return 1;
     }
     else {
       return 0;
     }
  }
  else if (strcmp (name, "dump") == 0)  {
     Process *p = (Process *) dp->getPtrParam ("proc");
     FILE *fp = (FILE *) dp->getPtrParam ("outfp");
     Assert (p && fp, "What?");
     Spef *spf = (Spef *) dp->getMap (p);
     if (!spf) { return 0; }
     spf->dumpRC (fp);
     // dump spef parasitics to file!
     return 1;
  }
  else {
     warning ("annotate: runcmd, unknown command `%s'", name);
     return 0;
  }
}
