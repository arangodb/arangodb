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

// Evaluates the cross product of unit-length vectors "a" and "b" in a
// numerically stable way, returning true if the error in the result is
// guaranteed to be at most kRobustCrossProdError.
template <class T>
bool GetStableCrossProd(const Vector3<T>& a, const Vector3<T>& b,
                        Vector3<T>* result);

// Returns the cross product of two points computed using exact arithmetic and
// then symbolic perturbations if necessary, rounded to double-precision and
// scaled so that the result can be normalized to an S2Point with loss of
// precision due to floating-point underflow.
//
// REQUIRES: a != b (this case should be handled before calling this function)
Vector3_d ExactCrossProd(const S2Point& a, const S2Point& b);

// The maximum error in the method above.
extern const S1Angle kExactCrossProdError;

// Returns the cross product of two points using symbolic perturbations, rounded
// to double-precision and scaled so that the result can be normalized to an
// S2Point with loss of precision due to floating-point underflow.
//
// REQUIRES: a != b
// REQUIRES: a and b are linearly dependent
Vector3_d SymbolicCrossProd(const S2Point& a, const S2Point& b);

// Returns the intersection point of two edges computed using exact arithmetic
// and rounded to the nearest representable S2Point.
S2Point GetIntersectionExact(const S2Point& a0, const S2Point& a1,
                             const S2Point& b0, const S2Point& b1);

// The maximum error in the method above.
extern const S1Angle kIntersectionExactError;

// The following field is used exclusively by s2edge_crossings_test.cc to
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

// The following classes are used as template arguments to S2EdgeCrosserBase in
// order to create two versions, namely S2EdgeCrosser itself (which takes
// (const S2Point *) arguments and requires points to be stored persistently in
// memory) and S2CopyingEdgeCrosser (which takes (const S2Point&) arguments and
// makes its own copies of all points).
//
// These classes are intended to be used like pointers, i.e. operator* and
// operator-> return the S2Point value.  They also define an implicit
// conversion operator that returns the underlying representation as either a
// const pointer or const reference.

class S2Point_PointerRep {
 public:
  using T = const S2Point *;
  S2Point_PointerRep() : p_(nullptr) {}
  explicit S2Point_PointerRep(const S2Point* p) : p_(p) {}
  S2Point_PointerRep& operator=(const S2Point* p) { p_ = p; return *this; }
  operator const S2Point*() const { return p_; }  // Conversion operator.
  const S2Point& operator*() const { return *p_; }
  const S2Point* operator->() const { return p_; }

 private:
  const S2Point* p_;
};

class S2Point_ValueRep {
 public:
  using T = const S2Point &;
  S2Point_ValueRep() : p_() {}
  explicit S2Point_ValueRep(const S2Point& p) : p_(p) {}
  S2Point_ValueRep& operator=(const S2Point& p) { p_ = p; return *this; }
  operator const S2Point&() const { return p_; }  // Conversion operator.
  const S2Point& operator*() const { return p_; }
  const S2Point* operator->() const { return &p_; }

 private:
  S2Point p_;
};

}  // namespace internal
}  // namespace S2

#endif  // S2_S2EDGE_CROSSINGS_INTERNAL_H_
