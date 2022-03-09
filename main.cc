#include <stdio.h>
#include "spef.h"


int main (void)
{
  Spef *s = new Spef();
  if (!s->Read (stdin)) {
    printf ("Read error!\n");
  }

  s->Print (stdout);

  delete s;
  return 0;
}
