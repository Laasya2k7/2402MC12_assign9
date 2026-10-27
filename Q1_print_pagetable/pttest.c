
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

void
show_table(char *message)
{
  printf("\n========== %s ==========\n", message);
  vmprint();
}

int
main(void)
{
  char *p;

  show_table("Initial Page Table");

  // Allocate exactly four pages.
  p = sbrk(4 * PGSIZE);
  if (p == (char *)-1) {
    printf("sbrk allocation failed\n");
    exit(1);
  }

  // Touch each of the four allocated pages.
  for (int i = 0; i < 4; i++)
    p[i * PGSIZE] = 'A';

  show_table("After Allocating 4 Pages");

  exit(0);
}
