/* ----------------------------------------------------------------------------
Copyright (c) 2018, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/*
Testing allocators is difficult as bugs may only surface after particular
allocation patterns. The main approach to testing _mimalloc_ is therefore
to have extensive internal invariant checking (see `page_is_valid` in `page.c`
for example), which is enabled in debug mode with `-DMI_CHECK_FULL=ON`.
The main testing is then to run `mimalloc-bench` [1] using full invariant checking
to catch any potential problems over a wide range of intensive allocation bench
marks.

However, this does not test well for the entire API surface. In this test file
we therefore test the API over various inputs. Please add more tests :-)

[1] https://github.com/daanx/mimalloc-bench
*/

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include "mimalloc.h"
#include "mimalloc-internal.h"

// ---------------------------------------------------------------------------
// Test macros: CHECK(name,predicate) and CHECK_BODY(name,body)
// ---------------------------------------------------------------------------
static int ok = 0;
static int failed = 0;

#define CHECK_BODY(name,body) \
 do { \
  fprintf(stderr,"test: %s...  ", name ); \
  bool result = true;                                     \
  do { body } while(false);                                \
  if (!(result)) {                                        \
    failed++; \
    fprintf(stderr,                                       \
            "\n  FAILED: %s:%d:\n  %s\n",                 \
            __FILE__,                                     \
            __LINE__,                                     \
            #body);                                       \
    /* exit(1); */ \
  } \
  else { \
    ok++;                               \
    fprintf(stderr,"ok.\n");                    \
  }                                             \
 } while (false)

#define CHECK(name,expr)      CHECK_BODY(name,{ result = (expr); })

// ---------------------------------------------------------------------------
// Test functions
// ---------------------------------------------------------------------------
bool test_heap1();
bool test_heap2();

// ---------------------------------------------------------------------------
// Main testing
// ---------------------------------------------------------------------------
int main() {
  mi_option_disable(mi_option_verbose);

  // ---------------------------------------------------
  // Malloc
  // ---------------------------------------------------

  CHECK_BODY("malloc-zero",{
    void* p = mi_malloc(0); mi_free(p);
  });
  CHECK_BODY("malloc-nomem1",{
    result = (mi_malloc(SIZE_MAX/2) == NULL);
  });
  CHECK_BODY("malloc-null",{
    mi_free(NULL);
  });

  // ---------------------------------------------------
  // Extended
  // ---------------------------------------------------
  #if defined(MI_MALLOC_OVERRIDE) && !defined(_WIN32)
  CHECK_BODY("posix_memalign1", {
    void* p = &p;
    int err = posix_memalign(&p, sizeof(void*), 32);
    mi_assert((err==0 && (uintptr_t)p % sizeof(void*) == 0) || p==&p);
    mi_free(p);
    result = (err==0);
  });
  CHECK_BODY("posix_memalign_no_align", {
    void* p = &p;
    int err = posix_memalign(&p, 3, 32);
    mi_assert(p==&p);
    result = (err==EINVAL);
  });
  CHECK_BODY("posix_memalign_zero", {
    void* p = &p;
    int err = posix_memalign(&p, sizeof(void*), 0);
    mi_free(p);
    result = (err==0);
  });
  CHECK_BODY("posix_memalign_nopow2", {
    void* p = &p;
    int err = posix_memalign(&p, 3*sizeof(void*), 32);
    result = (err==EINVAL && p==&p);
  });
  CHECK_BODY("posix_memalign_nomem", {
    void* p = &p;
    int err = posix_memalign(&p, sizeof(void*), SIZE_MAX);
    result = (err==ENOMEM && p==&p);
  });
  #endif

  // ---------------------------------------------------
  // Aligned API
  // ---------------------------------------------------
  CHECK_BODY("malloc-aligned1", {
    void* p = mi_malloc_aligned(32,32); result = (p != NULL && (uintptr_t)(p) % 32 == 0); mi_free(p);
  });
  CHECK_BODY("malloc-aligned2", {
    void* p = mi_malloc_aligned(48,32); result = (p != NULL && (uintptr_t)(p) % 32 == 0); mi_free(p);
  });
  CHECK_BODY("malloc-aligned-at1", {
    void* p = mi_malloc_aligned_at(48,32,0); result = (p != NULL && ((uintptr_t)(p) + 0) % 32 == 0); mi_free(p);
  });
  CHECK_BODY("malloc-aligned-at2", {
    void* p = mi_malloc_aligned_at(50,32,8); result = (p != NULL && ((uintptr_t)(p) + 8) % 32 == 0); mi_free(p);
  });

  // ---------------------------------------------------
  // Heaps
  // ---------------------------------------------------
  CHECK("heap_destroy", test_heap1());
  CHECK("heap_delete", test_heap2());

  //mi_stats_print(NULL);

  // ---------------------------------------------------
  // various
  // ---------------------------------------------------
  CHECK_BODY("realpath", {
    char* s = mi_realpath( ".", NULL );
    // printf("realpath: %s\n",s);
    mi_free(s);
  });

  // ---------------------------------------------------
  // Done
  // ---------------------------------------------------[]
  fprintf(stderr,"\n\n---------------------------------------------\n"
                 "succeeded: %i\n"
                 "failed   : %i\n\n", ok, failed);
  return failed;
}

// ---------------------------------------------------
// Larger test functions
// ---------------------------------------------------

bool test_heap1() {
  mi_heap_t* heap = mi_heap_new();
  int* p1 = mi_heap_malloc_tp(heap,int);
  int* p2 = mi_heap_malloc_tp(heap,int);
  *p1 = *p2 = 43;
  mi_heap_destroy(heap);
  return true;
}

bool test_heap2() {
  mi_heap_t* heap = mi_heap_new();
  int* p1 = mi_heap_malloc_tp(heap,int);
  int* p2 = mi_heap_malloc_tp(heap,int);
  mi_heap_delete(heap);
  *p1 = 42;
  mi_free(p1);
  mi_free(p2);
  return true;
}
