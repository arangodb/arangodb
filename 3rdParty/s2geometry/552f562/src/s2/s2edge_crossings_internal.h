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
//
// The following functions are not part of the public API.  Currently they are
// only used internally for testing purposes.

#ifndef S2_S2EDGE_CROSSINGS_INTERNAL_H_
#define S2_S2EDGE_CROSSINGS_INTERNAL_H_

#include "s2/s1angle.h"
#include "s2/s2point.h"

namespace S2 {
namespace internal {

// Returns the intersection point of two edges computed using exact arithmetic
// and rounded to the nearest representable S2Point.
S2Point GetIntersectionExact(const S2Point& a0, const S2Point& a1,
                             const S2Point& b0, const S2Point& b1);

// The maximum error in the method above.
extern const S1Angle kIntersectionExactError;

// The following field is used exclusively by S2EdgeUtilTesting in order to
// measure how often each intersection method is used by GetIntersection().
// If non-nullptr, then it points to an array of integers indexed by an
// IntersectionMethod enum value.  Each call to GetIntersection() increments
// the array entry corresponding to the intersection method that was used.
extern int* intersection_method_tally_;

// The list of intersection methods implemented by GetIntersection().
enum class IntersectionMethod {
  SIMPLE,
  SIMPLE_LD,
  STABLE,
  STABLE_LD,
  EXACT,
  NUM_METHODS
};
const char* GetIntersectionMethodName(IntersectionMethod method);

}  // namespace internal
}  // namespace S2

#endif  // S2_S2EDGE_CROSSINGS_INTERNAL_H_
