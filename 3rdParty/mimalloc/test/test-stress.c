/* ----------------------------------------------------------------------------
Copyright (c) 2018,2019 Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license.
-----------------------------------------------------------------------------*/

/* This is a stress test for the allocator, using multiple threads and
   transferring objects between threads. This is not a typical workload
   but uses a random linear size distribution. Timing can also depend on
   (random) thread scheduling. Do not use this test as a benchmark!
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <mimalloc.h>

// > mimalloc-test-stress [THREADS] [SCALE] [ITER]
//
// argument defaults
static int THREADS = 32;      // more repeatable if THREADS <= #processors
static int SCALE   = 50;      // scaling factor
static int ITER    = 10;      // N full iterations re-creating all threads

// static int THREADS = 8;    // more repeatable if THREADS <= #processors
// static int SCALE   = 100;  // scaling factor

static bool   allow_large_objects = true;    // allow very large objects?
static size_t use_one_size = 0;              // use single object size of N uintptr_t?


#ifdef USE_STD_MALLOC
#define custom_malloc(s)      malloc(s)
#define custom_realloc(p,s)   realloc(p,s)
#define custom_free(p)        free(p)
#else
#define custom_malloc(s)      mi_malloc(s)
#define custom_realloc(p,s)   mi_realloc(p,s)
#define custom_free(p)        mi_free(p)
#endif

// transfer pointer between threads
#define TRANSFERS     (1000)
static volatile void* transfer[TRANSFERS];


#if (UINTPTR_MAX != UINT32_MAX)
const uintptr_t cookie = 0xbf58476d1ce4e5b9UL;
#else
const uintptr_t cookie = 0x1ce4e5b9UL;
#endif

static void* atomic_exchange_ptr(volatile void** p, void* newval);

typedef uintptr_t* random_t;

static uintptr_t pick(random_t r) {
  uintptr_t x = *r;
#if (UINTPTR_MAX > UINT32_MAX)
  // by Sebastiano Vigna, see: <http://xoshiro.di.unimi.it/splitmix64.c>
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9UL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebUL;
  x ^= x >> 31;
#else
  // by Chris Wellons, see: <https://nullprogram.com/blog/2018/07/31/>
  x ^= x >> 16;
  x *= 0x7feb352dUL;
  x ^= x >> 15;
  x *= 0x846ca68bUL;
  x ^= x >> 16;
#endif
  *r = x;
  return x;
}

static bool chance(size_t perc, random_t r) {
  return (pick(r) % 100 <= perc);
}

static void* alloc_items(size_t items, random_t r) {
  if (chance(1, r)) {
    if (chance(1, r) && allow_large_objects) items *= 10000;       // 0.01% giant
    else if (chance(10, r) && allow_large_objects) items *= 1000;  // 0.1% huge
    else items *= 100;                                             // 1% large objects;
  }
  if (items == 40) items++;              // pthreads uses that size for stack increases
  if (use_one_size > 0) items = (use_one_size / sizeof(uintptr_t));
  uintptr_t* p = (uintptr_t*)custom_malloc(items * sizeof(uintptr_t));
  if (p != NULL) {
    for (uintptr_t i = 0; i < items; i++) p[i] = (items - i) ^ cookie;
  }
  return p;
}

static void free_items(void* p) {
  if (p != NULL) {
    uintptr_t* q = (uintptr_t*)p;
    uintptr_t items = (q[0] ^ cookie);
    for (uintptr_t i = 0; i < items; i++) {
      if ((q[i] ^ cookie) != items - i) {
        fprintf(stderr, "memory corruption at block %p at %zu\n", p, i);
        abort();
      }
    }
  }
  custom_free(p);
}


static void stress(intptr_t tid) {
  //bench_start_thread();
  uintptr_t r = tid * 43;
  const size_t max_item_shift = 5; // 128  
  const size_t max_item_retained_shift = max_item_shift + 2;
  size_t allocs = 100 * ((size_t)SCALE) * (tid % 8 + 1); // some threads do more
  size_t retain = allocs / 2;
  void** data = NULL;
  size_t data_size = 0;
  size_t data_top = 0;
  void** retained = (void**)custom_malloc(retain * sizeof(void*));
  size_t retain_top = 0;

  while (allocs > 0 || retain > 0) {
    if (retain == 0 || (chance(50, &r) && allocs > 0)) {
      // 50%+ alloc
      allocs--;
      if (data_top >= data_size) {
        data_size += 100000;
        data = (void**)custom_realloc(data, data_size * sizeof(void*));
      }
      data[data_top++] = alloc_items( 1ULL << (pick(&r) % max_item_shift), &r);
    }
    else {
      // 25% retain
      retained[retain_top++] = alloc_items( 1ULL << (pick(&r) % max_item_retained_shift), &r);
      retain--;
    }
    if (chance(66, &r) && data_top > 0) {
      // 66% free previous alloc
      size_t idx = pick(&r) % data_top;
      free_items(data[idx]);
      data[idx] = NULL;
    }
    if (chance(25, &r) && data_top > 0) {
      // 25% exchange a local pointer with the (shared) transfer buffer.
      size_t data_idx = pick(&r) % data_top;
      size_t transfer_idx = pick(&r) % TRANSFERS;
      void* p = data[data_idx];
      void* q = atomic_exchange_ptr(&transfer[transfer_idx], p);
      data[data_idx] = q;
    }
  }
  // free everything that is left
  for (size_t i = 0; i < retain_top; i++) {
    free_items(retained[i]);
  }
  for (size_t i = 0; i < data_top; i++) {
    free_items(data[i]);
  }
  custom_free(retained);
  custom_free(data);
  //bench_end_thread();
}

static void run_os_threads(size_t nthreads);

int main(int argc, char** argv) {
  // > mimalloc-test-stress [THREADS] [SCALE] [ITER]
  if (argc >= 2) {
    char* end;
    long n = strtol(argv[1], &end, 10);
    if (n > 0) THREADS = n;
  }
  if (argc >= 3) {
    char* end;
    long n = (strtol(argv[2], &end, 10));
    if (n > 0) SCALE = n;
  }
  if (argc >= 4) {
    char* end;
    long n = (strtol(argv[3], &end, 10));
    if (n > 0) ITER = n;
  }
  printf("start with %d threads with a %d%% load-per-thread and %d iterations\n", THREADS, SCALE, ITER);
  //int res = mi_reserve_huge_os_pages(4,1);
  //printf("(reserve huge: %i\n)", res);

  //bench_start_program();

  // Run ITER full iterations where half the objects in the transfer buffer survive to the next round.
  mi_stats_reset();
  uintptr_t r = 43 * 43;
  for (int n = 0; n < ITER; n++) {
    run_os_threads(THREADS);
    for (int i = 0; i < TRANSFERS; i++) {
      if (chance(50, &r) || n + 1 == ITER) { // free all on last run, otherwise free half of the transfers
        void* p = atomic_exchange_ptr(&transfer[i], NULL);
        free_items(p);
      }
    }
    mi_collect(false);
#ifndef NDEBUG
    if ((n + 1) % 10 == 0) { printf("- iterations: %3d\n", n + 1); }
#endif
  }

  mi_collect(true);
  mi_stats_print(NULL);
  //bench_end_program();
  return 0;
}


#ifdef _WIN32

#include <windows.h>

static DWORD WINAPI thread_entry(LPVOID param) {
  stress((intptr_t)param);
  return 0;
}

static void run_os_threads(size_t nthreads) {
  DWORD* tids = (DWORD*)custom_malloc(nthreads * sizeof(DWORD));
  HANDLE* thandles = (HANDLE*)custom_malloc(nthreads * sizeof(HANDLE));
  for (uintptr_t i = 0; i < nthreads; i++) {
    thandles[i] = CreateThread(0, 4096, &thread_entry, (void*)(i), 0, &tids[i]);
  }
  for (size_t i = 0; i < nthreads; i++) {
    WaitForSingleObject(thandles[i], INFINITE);
  }
  for (size_t i = 0; i < nthreads; i++) {
    CloseHandle(thandles[i]);
  }
  custom_free(tids);
  custom_free(thandles);
}

static void* atomic_exchange_ptr(volatile void** p, void* newval) {
#if (INTPTR_MAX == UINT32_MAX)
  return (void*)InterlockedExchange((volatile LONG*)p, (LONG)newval);
#else
  return (void*)InterlockedExchange64((volatile LONG64*)p, (LONG64)newval);
#endif
}
#else

#include <pthread.h>
#include <stdatomic.h>

static void* thread_entry(void* param) {
  stress((uintptr_t)param);
  return NULL;
}

static void run_os_threads(size_t nthreads) {
  pthread_t* threads = (pthread_t*)custom_malloc(nthreads * sizeof(pthread_t));
  memset(threads, 0, sizeof(pthread_t) * nthreads);
  //pthread_setconcurrency(nthreads);
  for (uintptr_t i = 0; i < nthreads; i++) {
    pthread_create(&threads[i], NULL, &thread_entry, (void*)i);
  }
  for (size_t i = 0; i < nthreads; i++) {
    pthread_join(threads[i], NULL);
  }
  custom_free(threads);
}

static void* atomic_exchange_ptr(volatile void** p, void* newval) {
  return atomic_exchange_explicit((volatile _Atomic(void*)*)p, newval, memory_order_acquire);
}

#endif
