// Copyright 2005 Google Inc. All Rights Reserved.
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

//
//
// Data transforms that can help code more efficiently.

#ifndef S2_UTIL_CODING_TRANSFORMS_H_
#define S2_UTIL_CODING_TRANSFORMS_H_

#include "s2/base/integral_types.h"

// ZigZag Transform
//
// Good for varint coding small signed integers centered around 0.
//
//       int32 ->     uint32
// -------------------------
//           0 ->          0
//          -1 ->          1
//           1 ->          2
//          -2 ->          3
//         ... ->        ...
//  2147483647 -> 4294967294
// -2147483648 -> 4294967295
//
//        >> encode >>
//        << decode <<

static inline uint32 ZigZagEncode(int32 n) {
  // We need the cast to avoid an arithmetic shift.
  uint32 sign = (static_cast<uint32>(n)) >> 31;
  return (static_cast<uint32>(n) << 1) ^ (0u - sign);
}

static inline int32 ZigZagDecode(uint32 n) {
  return (n >> 1) ^ (0u - (n & 1));
}

static inline uint64 ZigZagEncode64(int64 n) {
  // We need the cast to avoid an arithmetic shift.
  uint64 sign = (static_cast<uint64>(n)) >> 63;
  return (static_cast<uint64>(n) << 1) ^ (0u - sign);
}

static inline int64 ZigZagDecode64(uint64 n) {
  return (n >> 1) ^ (0u - (n & 1));
}

#endif  // S2_UTIL_CODING_TRANSFORMS_H_
