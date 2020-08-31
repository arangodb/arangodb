////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author http://en.wikipedia.org/wiki/Binary_GCD_algorithm
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_GCD_H
#define ARANGODB_BASICS_GCD_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief binary greatest common divisor
/// @author http://en.wikipedia.org/wiki/Binary_GCD_algorithm
/// note: T must be an unsigned type
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static T binaryGcd(T u, T v) {
  if (u == 0) {
    return v;
  }
  if (v == 0) {
    return u;
  }

  int shift;
  for (shift = 0; ((u | v) & 1) == 0; ++shift) {
    u >>= 1;
    v >>= 1;
  }

  while ((u & 1) == 0) {
    u >>= 1;
  }

  do {
    while ((v & 1) == 0) {
      v >>= 1;
    }

    if (u > v) {
      std::swap(v, u);
    }
    v = v - u;
  } while (v != 0);

  return u << shift;
}
}  // namespace basics
}  // namespace arangodb

#endif
