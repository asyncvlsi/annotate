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
#include "sdf.h"

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


static bool load_spef (ActDynamicPass *dp)
{
  Spef *spf;
  
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
	delete spf;
	return false;
      }
    }
    else {
      fp = fopen (buf, "r");
      if (!fp) {
	delete spf;
	return false;
      }
    }
    spf->Read (fp);
    fclose (fp);
    dp->setParam ("spef", spf);
  }
  return true;
}


static bool load_sdf (ActDynamicPass *dp)
{
  SDF *sdf;
  
  if (!dp->hasParam ("sdf")) {
    char buf[1024];
    char *ns = NULL;
    Process *root = dp->getRoot();
    FILE *fp;
    
    Assert(root, "What?");
    if (root->getns() && root->getns() != ActNamespace::Global()) {
      ns = root->getns()->Name();
    }
    if (ns) {
      snprintf (buf, 1024, "%s::%s.sdf", ns, root->getName());
    }
    else {
      snprintf (buf, 1024, "%s.sdf", root->getName());
    }
    if (ns) {
      FREE (ns);
    }
    sdf = new SDF (true);
    if (config_exists (buf)) {
      fp = fopen (config_get_string (buf), "r");
      if (!fp) {
	warning ("Could not open SDF file `%s' for reading",
		 config_get_string (buf));
	delete sdf;
	return false;
      }
    }
    else {
      fp = fopen (buf, "r");
      if (!fp) {
	delete sdf;
	return false;
      }
    }
    sdf->Read (fp);
    fclose (fp);
    dp->setParam ("sdf", sdf);
  }
  return true;
}  


/* Not defining this
void annotate_pass_run (ActPass *ap, Process *p)
void annotate_pass_recursive (ActPass *ap, Process *p, int mode);
*/

void *annotate_pass_proc (ActPass *ap, Process *p, int mode)
{
  char * s= NULL;
  Spef *spf = NULL;
  SDF *sdf = NULL;
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *> (ap);

  if (!p || !dp->getRoot()) {
    /* run on the top level global namespace */
    printf ("Annotation pass must be run with a specified top-level process only.");
    return NULL;
  }

  if (dp->getRoot() == p) {
    /* load in SPEF and/or SDF files for the top-level process */
    unsigned int flags;

    if (dp->hasParam ("annotate")) {
      flags = dp->getIntParam ("annotate");
    }
    else {
      flags = 0;
    }
    if (flags & 1) {
      if (!load_spef (dp)) {
	warning ("Annotation pass: SPEF reading failed!");
      }
    }
    if (flags & 2) {
      if (!load_sdf (dp)) {
	warning ("Annotation pass: SDF reading failed!");
      }
    }
  }
  spf = (Spef *) dp->getPtrParam ("spef");
  sdf = (SDF *) dp->getPtrParam ("sdf");
  if (!spf && !sdf) {
    return NULL;
  }

  /* For SPEF files, we need to organize everything by hierarchy!
     --> resistance should be consistent per cell
     --> cap should be broken down into local + residual
  */

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
