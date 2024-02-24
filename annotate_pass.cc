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
#include <act/act.h>
#include <act/passes.h>
#include "spef.h"

void annotate_pass_init (ActPass *ap)
{
  Act *a = ap->getAct();
  ActDynamicPass *adp = dynamic_cast<ActDynamicPass *>(ap);
  Assert (adp, "Hmm..");

  ActPass *nlp = a->pass_find ("prs2net");

  if (!nlp) {
    nlp = new ActNetlistPass (a);
  }
  adp->addDependency ("prs2net");
}

/* Not defining this
void annotate_pass_run (ActPass *ap, Process *p)
void annotate_pass_recursive (ActPass *ap, Process *p, int mode);
*/

void *annotate_pass_proc (ActPass *ap, Process *p, int mode)
{
  char * s= NULL;
  Spef *spf = NULL;
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *> (ap);

  if (!p || !dp->getRoot()) {
    /* run on the top level global namespace */
    printf ("SPEF pass must be run with a specified top-level process only.");
    return NULL;
  }
  
  if (!dp->hasParam ("spef")) {
    char buf[1024];
    char *ns = NULL;
    Process *root = dp->getRoot();
    FILE *fp;
    
    Assert(root, "What?");
    if (root->getns() && root->getns() != ActNamespace::Global()) {
      ns = root->getns()->Name();
    }
    if (ns) {
      snprintf (buf, 1024, "%s::%s.spef", ns, root->getName());
    }
    else {
      snprintf (buf, 1024, "%s.spef", root->getName());
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
	return NULL;
      }
    }
    else {
      fp = fopen (buf, "r");
      if (!fp) {
	warning ("Could not open SPEF file `%s' for reading", buf);
	return NULL;
      }
    }
    spf->Read (fp);
    fclose (fp);
    dp->setParam ("spef", spf);
  }
  else {
    spf = (Spef *) dp->getPtrParam ("spef");
  }
  Assert (spf, "What?!");

  
  return NULL;
}

void annotate_pass_free (ActPass *ap, void *v)
{
  /* this should be NULL, ignore it */
}


void annotate_pass_done (ActPass *ap)
{
  printf ("Pass deleted!\n");
}
