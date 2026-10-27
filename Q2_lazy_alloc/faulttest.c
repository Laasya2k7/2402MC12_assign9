
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

int
main(void)
{
  char *p;

  printf("Testing lazy allocation\n");

  int before = getfaults();

  p = sbrk(5 * PGSIZE);
  if (p == (char *)-1) {
    printf("sbrk failed\n");
    exit(1);
  }

  printf("Reserved 5 pages. Accessing them now...\n");

  for (int i = 0; i < 5; i++) {
    p[i * PGSIZE] = 'A';
  }

  int after = getfaults();

  printf("Page faults before: %d\n", before);
  printf("Page faults after: %d\n", after);
  printf("New page faults handled: %d\n", after - before);

  printf("Testing invalid address in a child process...\n");

  int pid = fork();

  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    volatile char *bad = p + 10 * PGSIZE;
    *bad = 'X';
    printf("ERROR: invalid access did not kill child\n");
    exit(1);
  } else {
    wait(0);
    printf("Parent survived child's invalid access\n");
  }

  exit(0);
}
