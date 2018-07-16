// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef S2_UTIL_MATH_VECTOR3_HASH_H_
#define S2_UTIL_MATH_VECTOR3_HASH_H_

#include <cstddef>
#include <functional>
#include <type_traits>

#include "s2/util/hash/mix.h"
#include "s2/util/math/vector.h"

template <class T>
struct GoodFastHash;

template <class VType>
struct GoodFastHash<Vector2<VType>> {
  std::size_t operator()(const Vector2<VType>& v) const {
    static_assert(std::is_pod<VType>::value, "POD expected");
    // std::hash collapses +/-0.
    std::hash<VType> h;
    HashMix mix(h(v.x()));
    mix.Mix(h(v.y()));
    return mix.get();
  }
};

template <class VType>
struct GoodFastHash<Vector3<VType>> {
  std::size_t operator()(const Vector3<VType>& v) const {
    static_assert(std::is_pod<VType>::value, "POD expected");
    // std::hash collapses +/-0.
    std::hash<VType> h;
    HashMix mix(h(v.x()));
    mix.Mix(h(v.y()));
    mix.Mix(h(v.z()));
    return mix.get();
  }
};

#endif  // S2_UTIL_MATH_VECTOR3_HASH_H_
