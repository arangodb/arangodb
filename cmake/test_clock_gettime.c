#include <stdio.h>
#include <time.h>

int main() {
  printf("-- testing clock_gettime\n");
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return 0;
}
