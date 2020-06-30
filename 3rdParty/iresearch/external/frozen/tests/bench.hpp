// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Extracted from google benchmarks suite
// c.f. https://github.com/google/benchmark/blob/master/include/benchmark/benchmark_api.h
//
// These two functions DoNotOptimize and ClobberMemory prevent the compiler
// from optimizing in certain ways.
//
// There is an excellent talk by Chandler Carruth about this here:
// https://www.youtube.com/watch?v=nXaxk27zwlk

#ifndef FROZEN_LETITGO_BENCH_HPP
#define FROZEN_LETITGO_BENCH_HPP

namespace benchmark {

#if defined(__GNUC__)
#define BENCHMARK_ALWAYS_INLINE __attribute__((always_inline))
#else
#define BENCHMARK_ALWAYS_INLINE
#endif

#if defined(__GNUC__)
template <class Tp>
inline BENCHMARK_ALWAYS_INLINE void
DoNotOptimize(Tp const & value) {
  asm volatile("" : : "g"(value) : "memory");
}
// Force the compiler to flush pending writes to global memory. Acts as an
// effective read/write barrier
inline BENCHMARK_ALWAYS_INLINE void
ClobberMemory() {
  asm volatile("" : : : "memory");
}

#else

template <class Tp>
inline BENCHMARK_ALWAYS_INLINE void
DoNotOptimize(Tp const & value) {
    static_cast<volatile void>(&reinterpret_cast<char const volatile&>(value));
}

// TODO
template <class Tp>
inline BENCHMARK_ALWAYS_INLINE void
ClobberMemory() {}

#endif

} // end namespace benchmark

#endif
