#include <stdio.h>
#include "spef.h"


int main (int argc, char **argv)
{
  Spef *s = new Spef(argc > 1 ? true : false);
  if (!s->Read (stdin)) {
    printf ("Read error!\n");
  }

  s->Print (stdout);

  delete s;
  return 0;
}
