// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef S2_S2SHAPEUTIL_CONTAINS_BRUTE_FORCE_H_
#define S2_S2SHAPEUTIL_CONTAINS_BRUTE_FORCE_H_

#include "s2/s2point.h"
#include "s2/s2shape_index.h"

namespace s2shapeutil {

// Returns true if the given shape contains the given point.  Most clients
// should not use this method, since its running time is linear in the number
// of shape edges.  Instead clients should create an S2ShapeIndex and use
// S2ContainsPointQuery, since this strategy is much more efficient when many
// points need to be tested.
//
// Polygon boundaries are treated as being semi-open (see S2ContainsPointQuery
// and S2VertexModel for other options).
//
// CAVEAT: Typically this method is only used internally.  Its running time is
//         linear in the number of shape edges.
bool ContainsBruteForce(const S2Shape& shape, const S2Point& point);

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_CONTAINS_BRUTE_FORCE_H_
