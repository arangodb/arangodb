#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <mimalloc.h>
#include <mimalloc-override.h>  // redefines malloc etc.

static void double_free1();
static void double_free2();
static void corrupt_free();

int main() {
  mi_version();
  
  // detect double frees and heap corruption
  // double_free1();
  // double_free2();
  // corrupt_free();

  void* p1 = malloc(78);
  void* p2 = malloc(24);
  free(p1);
  p1 = malloc(8);
  //char* s = strdup("hello\n");
  free(p2);
  p2 = malloc(16);
  p1 = realloc(p1, 32);
  free(p1);
  free(p2);
  //free(s);
  //mi_collect(true);

  /* now test if override worked by allocating/freeing across the api's*/
  //p1 = mi_malloc(32);
  //free(p1);
  //p2 = malloc(32);
  //mi_free(p2);
  mi_stats_print(NULL);
  return 0;
}


// The double free samples come ArcHeap [1] by Insu Yun (issue #161)
// [1]: https://arxiv.org/pdf/1903.00503.pdf

static void double_free1() {
  void* p[256];
  //uintptr_t buf[256];

  p[0] = mi_malloc(622616);
  p[1] = mi_malloc(655362);
  p[2] = mi_malloc(786432);
  mi_free(p[2]);
  // [VULN] Double free
  mi_free(p[2]);
  p[3] = mi_malloc(786456);
  // [BUG] Found overlap
  // p[3]=0x429b2ea2000 (size=917504), p[1]=0x429b2e42000 (size=786432)
  fprintf(stderr, "p3: %p-%p, p1: %p-%p, p2: %p\n", p[3], (uint8_t*)(p[3]) + 786456, p[1], (uint8_t*)(p[1]) + 655362, p[2]);
}

static void double_free2() {
  void* p[256];
  //uintptr_t buf[256];
  // [INFO] Command buffer: 0x327b2000
  // [INFO] Input size: 182
  p[0] = malloc(712352);
  p[1] = malloc(786432);
  free(p[0]);
  // [VULN] Double free
  free(p[0]);
  p[2] = malloc(786440);
  p[3] = malloc(917504);
  p[4] = malloc(786440);
  // [BUG] Found overlap
  // p[4]=0x433f1402000 (size=917504), p[1]=0x433f14c2000 (size=786432)
  fprintf(stderr, "p1: %p-%p, p2: %p-%p\n", p[4], (uint8_t*)(p[4]) + 917504, p[1], (uint8_t*)(p[1]) + 786432);
}


// Try to corrupt the heap through buffer overflow
#define N   256
#define SZ  64

static void corrupt_free() {
  void* p[N];
  // allocate
  for (int i = 0; i < N; i++) {
    p[i] = malloc(SZ);
  }
  // free some
  for (int i = 0; i < N; i += (N/10)) {
    free(p[i]);
    p[i] = NULL;
  }
  // try to corrupt the free list
  for (int i = 0; i < N; i++) {
    if (p[i] != NULL) {
      memset(p[i], 0, SZ+8);
    }
  }
  // allocate more.. trying to trigger an allocation from a corrupted entry
  // this may need many allocations to get there (if at all)
  for (int i = 0; i < 4096; i++) {
    malloc(SZ);
  }
}