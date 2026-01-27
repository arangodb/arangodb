// Copyright 2022 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Interface to vectorized quicksort with dynamic dispatch.
// Blog post: https://tinyurl.com/vqsort-blog
// Paper with measurements: https://arxiv.org/abs/2205.05982
//
// To ensure the overhead of using wide vectors (e.g. AVX2 or AVX-512) is
// worthwhile, we recommend using this code for sorting arrays whose size is at
// least 512 KiB.

#ifndef HIGHWAY_HWY_CONTRIB_SORT_VQSORT_H_
#define HIGHWAY_HWY_CONTRIB_SORT_VQSORT_H_

#include "hwy/base.h"

namespace hwy {

// Tag arguments that determine the sort order.
struct SortAscending {
  constexpr bool IsAscending() const { return true; }
};
struct SortDescending {
  constexpr bool IsAscending() const { return false; }
};

// Vectorized Quicksort: sorts keys[0, n). Dispatches to the best available
// instruction set and does not allocate memory.
// Uses about 1.2 KiB stack plus an internal 3-word TLS cache for random state.
HWY_CONTRIB_DLLEXPORT void VQSort(uint16_t* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(uint16_t* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(uint32_t* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(uint32_t* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(uint64_t* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(uint64_t* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(int16_t* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(int16_t* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(int32_t* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(int32_t* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(int64_t* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(int64_t* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(float* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(float* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(double* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(double* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(uint128_t* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(uint128_t* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(K64V64* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(K64V64* HWY_RESTRICT keys, size_t n,
                                  SortDescending);
HWY_CONTRIB_DLLEXPORT void VQSort(K32V32* HWY_RESTRICT keys, size_t n,
                                  SortAscending);
HWY_CONTRIB_DLLEXPORT void VQSort(K32V32* HWY_RESTRICT keys, size_t n,
                                  SortDescending);

// User-level caching is no longer required, so this class is no longer
// beneficial. We recommend using the simpler VQSort() interface instead, and
// retain this class only for compatibility. It now just calls VQSort.
class HWY_CONTRIB_DLLEXPORT Sorter {
 public:
  Sorter() {}
  ~Sorter() {}

  // Move-only
  Sorter(const Sorter&) = delete;
  Sorter& operator=(const Sorter&) = delete;
  Sorter(Sorter&& /*other*/) {}
  Sorter& operator=(Sorter&& /*other*/) { return *this; }

  void operator()(uint16_t* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(uint16_t* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(uint32_t* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(uint32_t* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(uint64_t* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(uint64_t* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }

  void operator()(int16_t* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(int16_t* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(int32_t* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(int32_t* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(int64_t* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(int64_t* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }

  void operator()(float* HWY_RESTRICT keys, size_t n, SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(float* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(double* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(double* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }

  void operator()(uint128_t* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(uint128_t* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }

  void operator()(K64V64* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(K64V64* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }

  void operator()(K32V32* HWY_RESTRICT keys, size_t n,
                  SortAscending tag) const {
    VQSort(keys, n, tag);
  }
  void operator()(K32V32* HWY_RESTRICT keys, size_t n,
                  SortDescending tag) const {
    VQSort(keys, n, tag);
  }

  // Unused
  static void Fill24Bytes(const void*, size_t, void*) {}
  static bool HaveFloat64() { return false; }

 private:
  void Delete() {}

  template <typename T>
  T* Get() const {
    return nullptr;
  }

#if HWY_COMPILER_CLANG
  HWY_DIAGNOSTICS(push)
  HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wunused-private-field")
#endif
  void* unused_ = nullptr;
#if HWY_COMPILER_CLANG
  HWY_DIAGNOSTICS(pop)
#endif
};

// Internal use only
HWY_CONTRIB_DLLEXPORT uint64_t* GetGeneratorState();

}  // namespace hwy

#endif  // HIGHWAY_HWY_CONTRIB_SORT_VQSORT_H_
