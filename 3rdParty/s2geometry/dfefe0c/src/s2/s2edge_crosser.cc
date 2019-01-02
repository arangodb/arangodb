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

#include "s2/s2edge_crosser.h"

#include "s2/base/logging.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"

int S2EdgeCrosser::CrossingSignInternal(const S2Point* d) {
  // Compute the actual result, and then save the current vertex D as the next
  // vertex C, and save the orientation of the next triangle ACB (which is
  // opposite to the current triangle BDA).
  int result = CrossingSignInternal2(*d);
  c_ = d;
  acb_ = -bda_;
  return result;
}

inline int S2EdgeCrosser::CrossingSignInternal2(const S2Point& d) {
  // At this point, a very common situation is that A,B,C,D are four points on
  // a line such that AB does not overlap CD.  (For example, this happens when
  // a line or curve is sampled finely, or when geometry is constructed by
  // computing the union of S2CellIds.)  Most of the time, we can determine
  // that AB and CD do not intersect by computing the two outward-facing
  // tangents at A and B (parallel to AB) and testing whether AB and CD are on
  // opposite sides of the plane perpendicular to one of these tangents.  This
  // is moderately expensive but still much cheaper than s2pred::ExpensiveSign.
  if (!have_tangents_) {
    S2Point norm = S2::RobustCrossProd(*a_, *b_).Normalize();
    a_tangent_ = a_->CrossProd(norm);
    b_tangent_ = norm.CrossProd(*b_);
    have_tangents_ = true;
  }
  // The error in RobustCrossProd() is insignificant.  The maximum error in
  // the call to CrossProd() (i.e., the maximum norm of the error vector) is
  // (0.5 + 1/sqrt(3)) * DBL_EPSILON.  The maximum error in each call to
  // DotProd() below is DBL_EPSILON.  (There is also a small relative error
  // term that is insignificant because we are comparing the result against a
  // constant that is very close to zero.)
  static const double kError = (1.5 + 1/sqrt(3)) * DBL_EPSILON;
  if ((c_->DotProd(a_tangent_) > kError && d.DotProd(a_tangent_) > kError) ||
      (c_->DotProd(b_tangent_) > kError && d.DotProd(b_tangent_) > kError)) {
    return -1;
  }

  // Otherwise, eliminate the cases where two vertices from different edges
  // are equal.  (These cases could be handled in the code below, but we would
  // rather avoid calling ExpensiveSign whenever possible.)
  if (*a_ == *c_ || *a_ == d || *b_ == *c_ || *b_ == d) return 0;

  // Eliminate cases where an input edge is degenerate.  (Note that in most
  // cases, if CD is degenerate then this method is not even called because
  // acb_ and bda have different signs.)
  if (*a_ == *b_ || *c_ == d) return -1;

  // Otherwise it's time to break out the big guns.
  if (acb_ == 0) acb_ = -s2pred::ExpensiveSign(*a_, *b_, *c_);
  S2_DCHECK_NE(acb_, 0);
  if (bda_ == 0) bda_ = s2pred::ExpensiveSign(*a_, *b_, d);
  S2_DCHECK_NE(bda_, 0);
  if (bda_ != acb_) return -1;

  Vector3_d c_cross_d = c_->CrossProd(d);
  int cbd = -s2pred::Sign(*c_, d, *b_, c_cross_d);
  S2_DCHECK_NE(cbd, 0);
  if (cbd != acb_) return -1;
  int dac = s2pred::Sign(*c_, d, *a_, c_cross_d);
  S2_DCHECK_NE(dac, 0);
  return (dac != acb_) ? -1 : 1;
}
