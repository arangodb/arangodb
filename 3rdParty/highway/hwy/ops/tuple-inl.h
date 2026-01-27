// Copyright 2023 Google LLC
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

// Tuple support. Included by those ops/* that lack native tuple types, after
// they define VFromD and before they use the tuples e.g. for LoadInterleaved2.
// Assumes we are already in the HWY_NAMESPACE and under an include guard.

// If viewing this header standalone, define VFromD to avoid IDE warnings.
// This is normally set by set_macros-inl.h before this header is included.
#if !defined(HWY_NAMESPACE)
#include "hwy/base.h"
template <class D>
using VFromD = int;
#endif

// On SVE, Vec2..4 are aliases to built-in types.
template <class D>
struct Vec2 {
  VFromD<D> v0;
  VFromD<D> v1;
};

template <class D>
struct Vec3 {
  VFromD<D> v0;
  VFromD<D> v1;
  VFromD<D> v2;
};

template <class D>
struct Vec4 {
  VFromD<D> v0;
  VFromD<D> v1;
  VFromD<D> v2;
  VFromD<D> v3;
};

// D arg is unused but allows deducing D.
template <class D>
HWY_API Vec2<D> Create2(D /* tag */, VFromD<D> v0, VFromD<D> v1) {
  return Vec2<D>{v0, v1};
}

template <class D>
HWY_API Vec3<D> Create3(D /* tag */, VFromD<D> v0, VFromD<D> v1, VFromD<D> v2) {
  return Vec3<D>{v0, v1, v2};
}

template <class D>
HWY_API Vec4<D> Create4(D /* tag */, VFromD<D> v0, VFromD<D> v1, VFromD<D> v2,
                        VFromD<D> v3) {
  return Vec4<D>{v0, v1, v2, v3};
}

template <size_t kIndex, class D>
HWY_API VFromD<D> Get2(Vec2<D> tuple) {
  static_assert(kIndex < 2, "Tuple index out of bounds");
  return kIndex == 0 ? tuple.v0 : tuple.v1;
}

template <size_t kIndex, class D>
HWY_API VFromD<D> Get3(Vec3<D> tuple) {
  static_assert(kIndex < 3, "Tuple index out of bounds");
  return kIndex == 0 ? tuple.v0 : kIndex == 1 ? tuple.v1 : tuple.v2;
}

template <size_t kIndex, class D>
HWY_API VFromD<D> Get4(Vec4<D> tuple) {
  static_assert(kIndex < 4, "Tuple index out of bounds");
  return kIndex == 0   ? tuple.v0
         : kIndex == 1 ? tuple.v1
         : kIndex == 2 ? tuple.v2
                       : tuple.v3;
}
