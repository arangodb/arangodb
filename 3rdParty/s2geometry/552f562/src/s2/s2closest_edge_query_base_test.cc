// Copyright 2013 Google Inc. All Rights Reserved.
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
// the actual algorithms is in s2closest_edge_query_test.cc.

#include "s2/s2closest_edge_query_base.h"

#include <vector>
#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2edge_distances.h"
#include "s2/s2text_format.h"

using std::vector;

namespace {

// This is a proof-of-concept prototype of a possible S2FurthestEdgeQuery
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
  // Expose only the methods that are documented as necessary in the API.
  class Delta {
   public:
    Delta() {}
    Delta(const Delta& x) : a_(x.a_) {}
    Delta& operator=(const Delta& x) { a_ = x.a_; return *this; }
    friend bool operator==(Delta x, Delta y) { return x.a_ == y.a_; }
    static Delta Zero() { Delta r; r.a_ = S1ChordAngle::Zero(); return r; }

    // This method is needed to implement Distance::operator-.
    explicit operator S1ChordAngle() const { return a_; }

   private:
    S1ChordAngle a_;
  };

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
  friend MaxDistance operator-(MaxDistance x, Delta delta) {
    return MaxDistance(x.distance_ + S1ChordAngle(delta));
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

using FurthestEdgeQuery = S2ClosestEdgeQueryBase<MaxDistance>;

class FurthestPointTarget final : public FurthestEdgeQuery::Target {
 public:
  explicit FurthestPointTarget(const S2Point& point) : point_(point) {}

  int max_brute_force_index_size() const override { return 100; }

  S2Cap GetCapBound() override {
    return S2Cap(-point_, S1ChordAngle::Zero());
  }

  bool UpdateMinDistance(const S2Point& p, MaxDistance* min_dist) override {
    return min_dist->UpdateMin(MaxDistance(S1ChordAngle(p, point_)));
  }

  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         MaxDistance* min_dist) override {
    S1ChordAngle dist180 =
        S1ChordAngle(*min_dist).is_negative() ? S1ChordAngle::Infinity() :
        S1ChordAngle::Straight() - S1ChordAngle(*min_dist);
    if (!S2::UpdateMinDistance(-point_, v0, v1, &dist180)) return false;
    *min_dist = MaxDistance(S1ChordAngle::Straight() - dist180);
    return true;
  }

  bool UpdateMinDistance(const S2Cell& cell,
                         MaxDistance* min_dist) override {
    return min_dist->UpdateMin(
        MaxDistance(S1ChordAngle::Straight() - cell.GetDistance(-point_)));
  }

  bool VisitContainingShapes(const S2ShapeIndex& index,
                             const ShapeVisitor& visitor) override {
    // For furthest points, we return the polygons whose interior contains the
    // antipode of the target point.  (These are the polygons whose
    // MaxDistance() to the target is MaxDistance::Zero().)
    //
    // For target types consisting of multiple connected components (such as
    // FurthestPointQuery::ShapeIndexTarget), this method should return the
    // polygons containing the antipodal reflection of any connected
    // component.  (It is sufficient to test containment of one vertex per
    // connected component, since the API allows us to also return any polygon
    // whose boundary has MaxDistance::Zero() to the target.)
    return MakeS2ContainsPointQuery(&index).VisitContainingShapes(
        -point_, [this, &visitor](S2Shape* shape) {
          return visitor(shape, point_);
        });
  }

 private:
  S2Point point_;
};

TEST(S2ClosestEdgeQueryBase, MaxDistance) {
  auto index = s2textformat::MakeIndex("0:0 | 1:0 | 2:0 | 3:0 # #");
  FurthestEdgeQuery query(index.get());
  FurthestPointTarget target(s2textformat::MakePoint("4:0"));
  FurthestEdgeQuery::Options options;
  options.set_max_edges(1);
  auto results = query.FindClosestEdges(&target, options);
  ASSERT_EQ(1, results.size());
  EXPECT_EQ(0, results[0].shape_id);
  EXPECT_EQ(0, results[0].edge_id);
  EXPECT_NEAR(4, S1ChordAngle(results[0].distance).ToAngle().degrees(), 1e-13);
}

}  // namespace
