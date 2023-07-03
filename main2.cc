#include <stdio.h>
#include "sdf.h"


int main (void)
{
  SDF *s = new SDF();
  if (!s->Read (stdin)) {
    printf ("Read error!\n");
  }

  s->Print (stdout);

  delete s;
  return 0;
}
