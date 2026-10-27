
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


#define MAX_FRAMES 5

#define N 20

int pages[N] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3,
                0, 3, 2, 1, 2, 0, 1, 7, 0, 1};

int frames[MAX_FRAMES];

int
fifo(int nf, int verbose)
{
  int faults = 0, hits = 0, next = 0;

  for (int i = 0; i < nf; i++)
    frames[i] = -1;

  if (verbose)
    printf("\n--- FIFO with %d frames ---\n", nf);

  for (int i = 0; i < N; i++) {
    int found = 0;

    for (int j = 0; j < nf; j++) {
      if (frames[j] == pages[i]) {
        found = 1;
        break;
      }
    }

    if (found) {
      hits++;
      if (verbose)
        printf("Page %d: HIT  ", pages[i]);
    } else {
      faults++;
      frames[next] = pages[i];
      next = (next + 1) % nf;
      if (verbose)
        printf("Page %d: FAULT ", pages[i]);
    }

    if (verbose) {
      printf("Frames: ");
      for (int j = 0; j < nf; j++) {
        if (frames[j] == -1)
          printf("- ");
        else
          printf("%d ", frames[j]);
      }
      printf("\n");
    }
  }

  if (verbose)
    printf("FIFO faults = %d, hits = %d, hit ratio = %d%%\n",
           faults, hits, hits * 100 / N);

  return faults;
}

int
lru(int nf, int verbose)
{
  int faults = 0, hits = 0;
  int last[MAX_FRAMES];

  for (int i = 0; i < nf; i++) {
    frames[i] = -1;
    last[i] = -1;
  }

  if (verbose)
    printf("\n--- LRU with %d frames ---\n", nf);

  for (int i = 0; i < N; i++) {
    int found = -1;

    for (int j = 0; j < nf; j++) {
      if (frames[j] == pages[i]) {
        found = j;
        break;
      }
    }

    if (found != -1) {
      hits++;
      last[found] = i;
      if (verbose)
        printf("Page %d: HIT  ", pages[i]);
    } else {
      faults++;
      int victim = -1;

      for (int j = 0; j < nf; j++) {
        if (frames[j] == -1) {
          victim = j;
          break;
        }
      }

      if (victim == -1) {
        victim = 0;
        for (int j = 1; j < nf; j++) {
          if (last[j] < last[victim])
            victim = j;
        }
      }

      frames[victim] = pages[i];
      last[victim] = i;

      if (verbose)
        printf("Page %d: FAULT ", pages[i]);
    }

    if (verbose) {
      printf("Frames: ");
      for (int j = 0; j < nf; j++) {
        if (frames[j] == -1)
          printf("- ");
        else
          printf("%d ", frames[j]);
      }
      printf("\n");
    }
  }

  if (verbose)
    printf("LRU faults = %d, hits = %d, hit ratio = %d%%\n",
           faults, hits, hits * 100 / N);

  return faults;
}

int
main(void)
{
  printf("Reference string: ");
  for (int i = 0; i < N; i++)
    printf("%d ", pages[i]);
  printf("\n");

  fifo(3, 1);
  lru(3, 1);

  printf("\n--- Comparison for the same reference string ---\n");
  printf("Frames  FIFO faults  FIFO hit%%  LRU faults  LRU hit%%\n");

  for (int f = 3; f <= 5; f++) {
    int ff = fifo(f, 0);
    int lf = lru(f, 0);

    printf("%d       %d            %d%%         %d           %d%%\n",
           f, ff, (N - ff) * 100 / N,
           lf, (N - lf) * 100 / N);
  }

  exit(0);
}
