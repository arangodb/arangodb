#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <mimalloc.h>

#include <new>

static void* p = malloc(8);

void free_p() {
  free(p);
  return;
}

class Test {
private:
  int i;
public:
  Test(int x) { i = x; }
  ~Test() { }
};

int main() {
  mi_stats_reset();
  atexit(free_p);
  void* p1 = malloc(78);
  void* p2 = malloc(24);
  free(p1);
  p1 = malloc(8);
  char* s = mi_strdup("hello\n");
  free(p2);
  p2 = malloc(16);
  p1 = realloc(p1, 32);
  free(p1);
  free(p2);
  free(s);
  Test* t = new Test(42);
  delete t;
  t = new (std::nothrow) Test(42);
  delete t;
  int err = mi_posix_memalign(&p1,32,60);
  if (!err) free(p1);
  free(p);
  mi_collect(true);
  mi_stats_print(NULL);  // MIMALLOC_VERBOSE env is set to 2
  return 0;
}

class Static {
private:
  void* p;
public:
  Static() {
    p = malloc(64);
    return;
  }
  ~Static() {
    free(p);
    return;
  }
};

static Static s = Static();
