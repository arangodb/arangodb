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
//
// This file contains some basic tests of the templating support.  Testing of
// the actual algorithms is in s2closest_point_query_test.cc.

#include "s2/s2closest_point_query_base.h"

#include <vector>
#include <gtest/gtest.h>
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2edge_distances.h"
#include "s2/s2text_format.h"

using std::vector;

namespace {

// This is a proof-of-concept prototype of a possible S2FurthestPointQuery
// class.  The purpose of this test is just to make sure that the code
// compiles and does something reasonable.  (A real implementation would need
// to be more careful about error bounds, it would implement a greater range
// of target types, etc.)
//
// It is based on the principle that for any two geometric objects X and Y on
// the sphere,
//
//     max_dist(X, Y) = Pi - min_dist(-X, Y)
//
// where "-X" denotes the reflection of X through the origin (i.e., to the
// opposite side of the sphere).

// MaxDistance is a class that allows maximum distances to be computed using a
// minimum distance algorithm.  It essentially treats a distance "x" as the
// supplementary distance (Pi - x).
class MaxDistance {
 public:
  using Delta = S1ChordAngle;

  MaxDistance() : distance_() {}
  explicit MaxDistance(S1ChordAngle x) : distance_(x) {}
  explicit operator S1ChordAngle() const { return distance_; }
  static MaxDistance Zero() {
    return MaxDistance(S1ChordAngle::Straight());
  }
  static MaxDistance Infinity() {
    return MaxDistance(S1ChordAngle::Negative());
  }
  static MaxDistance Negative() {
    return MaxDistance(S1ChordAngle::Infinity());
  }
  friend bool operator==(MaxDistance x, MaxDistance y) {
    return x.distance_ == y.distance_;
  }
  friend bool operator<(MaxDistance x, MaxDistance y) {
    return x.distance_ > y.distance_;
  }
  friend MaxDistance operator-(MaxDistance x, S1ChordAngle delta) {
    return MaxDistance(x.distance_ + delta);
  }
  S1ChordAngle GetChordAngleBound() const {
    return S1ChordAngle::Straight() - distance_;
  }

  // If (dist < *this), updates *this and returns true (used internally).
  bool UpdateMin(const MaxDistance& dist) {
    if (dist < *this) {
      *this = dist;
      return true;
    }
    return false;
  }

 private:
  S1ChordAngle distance_;
};

using FurthestPointQueryOptions = S2ClosestPointQueryBaseOptions<MaxDistance>;

class FurthestPointQueryTarget final : public S2DistanceTarget<MaxDistance> {
 public:
  explicit FurthestPointQueryTarget(const S2Point& point) : point_(point) {}

  int max_brute_force_index_size() const override { return 100; }

  S2Cap GetCapBound() override {
    return S2Cap(-point_, S1ChordAngle::Zero());
  }

  bool UpdateMinDistance(const S2Point& p, MaxDistance* min_dist) override {
    return min_dist->UpdateMin(MaxDistance(S1ChordAngle(p, point_)));
  }

  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         MaxDistance* min_dist) override {
    S2_LOG(FATAL) << "Unimplemented";
    return false;
  }

  bool UpdateMinDistance(const S2Cell& cell,
                         MaxDistance* min_dist) override {
    return min_dist->UpdateMin(
        MaxDistance(S1ChordAngle::Straight() - cell.GetDistance(-point_)));
  }

  bool VisitContainingShapes(const S2ShapeIndex& index,
                             const ShapeVisitor& visitor) override {
    S2_LOG(FATAL) << "Unimplemented";
    return false;
  }

 private:
  S2Point point_;
};

template <class Data>
using FurthestPointQuery = S2ClosestPointQueryBase<MaxDistance, Data>;

TEST(S2ClosestPointQueryBase, MaxDistance) {
  S2PointIndex<int> index;
  auto points = s2textformat::ParsePointsOrDie("0:0, 1:0, 2:0, 3:0");
  for (int i = 0; i < points.size(); ++i) {
    index.Add(points[i], i);
  }
  FurthestPointQuery<int> query(&index);
  FurthestPointQueryTarget target(s2textformat::MakePoint("4:0"));
  FurthestPointQueryOptions options;
  options.set_max_points(1);
  auto results = query.FindClosestPoints(&target, options);
  ASSERT_EQ(1, results.size());
  EXPECT_EQ(points[0], results[0].point());
  EXPECT_EQ(0, results[0].data());
  EXPECT_NEAR(4, S1ChordAngle(results[0].distance()).ToAngle().degrees(),
              1e-13);
}

}  // namespace
