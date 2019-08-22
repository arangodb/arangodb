/* ----------------------------------------------------------------------------
Copyright (c) 2018, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef MIMALLOC_ATOMIC_H
#define MIMALLOC_ATOMIC_H

// ------------------------------------------------------
// Atomics
// ------------------------------------------------------

// Atomically increment a value; returns the incremented result.
static inline uintptr_t mi_atomic_increment(volatile uintptr_t* p);

// Atomically increment a value; returns the incremented result.
static inline uint32_t mi_atomic_increment32(volatile uint32_t* p);

// Atomically decrement a value; returns the decremented result.
static inline uintptr_t mi_atomic_decrement(volatile uintptr_t* p);

// Atomically add a 64-bit value; returns the added result.
static inline int64_t mi_atomic_add(volatile int64_t* p, int64_t add);

// Atomically subtract a value; returns the subtracted result.
static inline uintptr_t mi_atomic_subtract(volatile uintptr_t* p, uintptr_t sub);

// Atomically subtract a value; returns the subtracted result.
static inline uint32_t mi_atomic_subtract32(volatile uint32_t* p, uint32_t sub);

// Atomically compare and exchange a value; returns `true` if successful.
static inline bool mi_atomic_compare_exchange32(volatile uint32_t* p, uint32_t exchange, uint32_t compare);

// Atomically compare and exchange a value; returns `true` if successful.
static inline bool mi_atomic_compare_exchange(volatile uintptr_t* p, uintptr_t exchange, uintptr_t compare);

// Atomically exchange a value.
static inline uintptr_t mi_atomic_exchange(volatile uintptr_t* p, uintptr_t exchange);

// Atomically read a value
static inline uintptr_t mi_atomic_read(volatile uintptr_t* p);

// Atomically write a value
static inline void mi_atomic_write(volatile uintptr_t* p, uintptr_t x);

// Atomically read a pointer
static inline void* mi_atomic_read_ptr(volatile void** p) {
  return (void*)mi_atomic_read( (volatile uintptr_t*)p );
}

static inline void mi_atomic_yield(void);


// Atomically write a pointer
static inline void mi_atomic_write_ptr(volatile void** p, void* x) {
  mi_atomic_write((volatile uintptr_t*)p, (uintptr_t)x );
}

// Atomically compare and exchange a pointer; returns `true` if successful.
static inline bool mi_atomic_compare_exchange_ptr(volatile void** p, void* newp, void* compare) {
  return mi_atomic_compare_exchange((volatile uintptr_t*)p, (uintptr_t)newp, (uintptr_t)compare);
}

// Atomically exchange a pointer value.
static inline void* mi_atomic_exchange_ptr(volatile void** p, void* exchange) {
  return (void*)mi_atomic_exchange((volatile uintptr_t*)p, (uintptr_t)exchange);
}


#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#if (MI_INTPTR_SIZE==8)
typedef LONG64   msc_intptr_t;
#define RC64(f)  f##64
#else
typedef LONG     msc_intptr_t;
#define RC64(f)  f
#endif
static inline uintptr_t mi_atomic_increment(volatile uintptr_t* p) {
  return (uintptr_t)RC64(_InterlockedIncrement)((volatile msc_intptr_t*)p);
}
static inline uint32_t mi_atomic_increment32(volatile uint32_t* p) {
  return (uint32_t)_InterlockedIncrement((volatile LONG*)p);
}
static inline uintptr_t mi_atomic_decrement(volatile uintptr_t* p) {
  return (uintptr_t)RC64(_InterlockedDecrement)((volatile msc_intptr_t*)p);
}
static inline uintptr_t mi_atomic_subtract(volatile uintptr_t* p, uintptr_t sub) {
  return (uintptr_t)RC64(_InterlockedExchangeAdd)((volatile msc_intptr_t*)p, -((msc_intptr_t)sub)) - sub;
}
static inline uint32_t mi_atomic_subtract32(volatile uint32_t* p, uint32_t sub) {
  return (uint32_t)_InterlockedExchangeAdd((volatile LONG*)p, -((LONG)sub)) - sub;
}
static inline bool mi_atomic_compare_exchange32(volatile uint32_t* p, uint32_t exchange, uint32_t compare) {
  return ((int32_t)compare == _InterlockedCompareExchange((volatile LONG*)p, (LONG)exchange, (LONG)compare));
}
static inline bool mi_atomic_compare_exchange(volatile uintptr_t* p, uintptr_t exchange, uintptr_t compare) {
  return (compare == RC64(_InterlockedCompareExchange)((volatile msc_intptr_t*)p, (msc_intptr_t)exchange, (msc_intptr_t)compare));
}
static inline uintptr_t mi_atomic_exchange(volatile uintptr_t* p, uintptr_t exchange) {
  return (uintptr_t)RC64(_InterlockedExchange)((volatile msc_intptr_t*)p, (msc_intptr_t)exchange);
}
static inline uintptr_t mi_atomic_read(volatile uintptr_t* p) {
  return *p;
}
static inline void mi_atomic_write(volatile uintptr_t* p, uintptr_t x) {
  *p = x;
}
static inline void mi_atomic_yield(void) {
  YieldProcessor();
}
static inline int64_t mi_atomic_add(volatile int64_t* p, int64_t add) {
  #if (MI_INTPTR_SIZE==8)
  return _InterlockedExchangeAdd64(p, add) + add;
  #else
  int64_t current;
  int64_t sum;
  do {
    current = *p;
    sum = current + add;
  } while (_InterlockedCompareExchange64(p, sum, current) != current);
  return sum;
  #endif
}

#else
#ifdef __cplusplus
#include <atomic>
#define  MI_USING_STD   using namespace std;
#define  _Atomic(tp)    atomic<tp>
#else
#include <stdatomic.h>
#define  MI_USING_STD
#endif
static inline uintptr_t mi_atomic_increment(volatile uintptr_t* p) {
  MI_USING_STD
  return atomic_fetch_add_explicit((volatile atomic_uintptr_t*)p, (uintptr_t)1, memory_order_relaxed) + 1;
}
static inline uint32_t mi_atomic_increment32(volatile uint32_t* p) {
  MI_USING_STD
  return atomic_fetch_add_explicit((volatile _Atomic(uint32_t)*)p, (uint32_t)1, memory_order_relaxed) + 1;
}
static inline uintptr_t mi_atomic_decrement(volatile uintptr_t* p) {
  MI_USING_STD
  return atomic_fetch_sub_explicit((volatile atomic_uintptr_t*)p, (uintptr_t)1, memory_order_relaxed) - 1;
}
static inline int64_t mi_atomic_add(volatile int64_t* p, int64_t add) {
  MI_USING_STD
  return atomic_fetch_add_explicit((volatile _Atomic(int64_t)*)p, add, memory_order_relaxed) + add;
}
static inline uintptr_t mi_atomic_subtract(volatile uintptr_t* p, uintptr_t sub) {
  MI_USING_STD
  return atomic_fetch_sub_explicit((volatile atomic_uintptr_t*)p, sub, memory_order_relaxed) - sub;
}
static inline uint32_t mi_atomic_subtract32(volatile uint32_t* p, uint32_t sub) {
  MI_USING_STD
  return atomic_fetch_sub_explicit((volatile _Atomic(uint32_t)*)p, sub, memory_order_relaxed) - sub;
}
static inline bool mi_atomic_compare_exchange32(volatile uint32_t* p, uint32_t exchange, uint32_t compare) {
  MI_USING_STD
  return atomic_compare_exchange_weak_explicit((volatile _Atomic(uint32_t)*)p, &compare, exchange, memory_order_release, memory_order_relaxed);
}
static inline bool mi_atomic_compare_exchange(volatile uintptr_t* p, uintptr_t exchange, uintptr_t compare) {
  MI_USING_STD
  return atomic_compare_exchange_weak_explicit((volatile atomic_uintptr_t*)p, &compare, exchange, memory_order_release, memory_order_relaxed);
}
static inline uintptr_t mi_atomic_exchange(volatile uintptr_t* p, uintptr_t exchange) {
  MI_USING_STD
  return atomic_exchange_explicit((volatile atomic_uintptr_t*)p, exchange, memory_order_acquire);
}
static inline uintptr_t mi_atomic_read(volatile uintptr_t* p) {
  MI_USING_STD
  return atomic_load_explicit((volatile atomic_uintptr_t*)p, memory_order_relaxed);
}
static inline void mi_atomic_write(volatile uintptr_t* p, uintptr_t x) {
  MI_USING_STD
  return atomic_store_explicit((volatile atomic_uintptr_t*)p, x, memory_order_relaxed);
}

#if defined(__cplusplus)
  #include <thread>
  static inline void mi_atomic_yield(void) {
    std::this_thread::yield();
  }
#elif (defined(__GNUC__) || defined(__clang__)) && \
      (defined(__x86_64__) || defined(__i386__) || defined(__arm__) || defined(__aarch64__))
#if defined(__x86_64__) || defined(__i386__)
  static inline void mi_atomic_yield(void) {
    asm volatile ("pause" ::: "memory");
  }
#elif defined(__arm__) || defined(__aarch64__)
  static inline void mi_atomic_yield(void) {
    asm volatile("yield");
  }
#endif
#elif defined(__wasi__)
  #include <sched.h>
  static inline void mi_atomic_yield() {
    sched_yield();
  }
#else
  #include <unistd.h>
  static inline void mi_atomic_yield(void) {
    sleep(0);
  }
#endif

#endif

#endif // __MIMALLOC_ATOMIC_H
