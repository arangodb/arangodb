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

// Author: ericv@google.com (Eric Veach)

#ifndef S2_S2POINT_H_
#define S2_S2POINT_H_

#include "s2/_fp_contract_off.h"
#include "s2/util/math/vector.h"  // IWYU pragma: export
#include "s2/util/math/vector3_hash.h"

// An S2Point represents a point on the unit sphere as a 3D vector.  Usually
// points are normalized to be unit length, but some methods do not require
// this.  See util/math/vector.h for the methods available.  Among other
// things, there are overloaded operators that make it convenient to write
// arithmetic expressions (e.g. (1-x)*p1 + x*p2).
using S2Point = Vector3_d;

// S2PointHash can be used with standard containers (e.g., unordered_set) or
// nonstandard extensions (e.g., hash_map).  It is defined such that if two
// S2Points compare equal to each other, they have the same hash.  (This
// requires that positive and negative zero hash to the same value.)
using S2PointHash = GoodFastHash<S2Point>;

#endif  // S2_S2POINT_H_
