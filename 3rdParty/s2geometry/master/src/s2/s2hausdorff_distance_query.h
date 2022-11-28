// Copyright 2022 Google Inc. All Rights Reserved.
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


#ifndef S2_S2HAUSDORFF_DISTANCE_QUERY_H_
#define S2_S2HAUSDORFF_DISTANCE_QUERY_H_

#include <algorithm>

#include "absl/types/optional.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2min_distance_targets.h"
#include "s2/s2point.h"
#include "s2/s2shape_index.h"

// S2HausdorffDistanceQuery is a helper class for computing discrete Hausdorff
// distances between two geometries. This can be useful for e.g. computing how
// far apart a highway and a semi-parallel frontage road ever get.
//
// Both geometries are provided as S2ShapeIndex objects. A S2ShapeIndex is a
// collection of points, polylines, and/or polygons. See s2shape_index.h for
// details.
//
// Discrete directed Hausdorff distance from target geometry A to source
// geometry B is defined as the maximum, over all vertices of A, of the closest
// edge distance from the vertex to geometry B.  It is called _discrete_ because
// the maximum is computed over all _vertices_ of A, rather than over other
// positions such as midpoints of the edges. The current implementation computes
// the discrete Hausdorff distance instead of exact Hausdorff distance because
// the latter incurs significantly larger computational expenses, while the
// former is suitable for most practical use cases.
//
// The undirected Hausdorff distance (usually referred to more simply as just
// Hausdorff distance) between geometries A and B is the maximum of the directed
// Hausdorff distances from A to B, and from B to A.
//
// The difference between directed and undirected Hausdorff distances can be
// illustrated by the following example. Let's say we have two polygonal
// geometries, one representing the continental contiguous United States, and
// the other Catalina island off the coast of California. The directed
// Hausdorff distance from the island to the continental US is going to be about
// 60 km - this is how far the furthest point on the island gets from the
// continent. At the same time, the directed Hausdorff distance from the
// continental US to Catalina island is several thousand kilometers - this is
// how far from the island one can get at the North-East corner of the US. The
// undirected Hausdorff distance between these two entities is the maximum of
// the two directed distances, that is a few thousand kilometers.
//
// For example, given two geometries A and B in the form of S2 shape indexes,
// the following code finds the directed Hausdorff distance from A to B and the
// [undirected] Hausdorff distance between A and B:
//
// bool ComputeHausdorffDistances(const S2ShapeIndex* A, const S2ShapeIndex* B,
//                                S1ChordAngle& directed_distance,
//                                S1ChordAngle& undirected_distance) {
//   S2HausdorffDistanceQuery query(S2HausdorffDistanceQuery::Options());
//
//   absl::optional<DirectedResult> directed_result =
//                  query.GetDirectedHausdorffDistance(A, B);
//   if (!directed_result) {
//     return false;
//   }
//   directed_distance = directed_result->distance();
//
//   absl::optional<Result> undirected_result =
//                  query.GetHausdorffDistance(A, B);
//   undirected_distance = undirected_result->distance();
//
//   return true;
// }
//
// For the definition of Hausdorff distance and other details see
// https://en.wikipedia.org/wiki/Hausdorff_distance.
//
class S2HausdorffDistanceQuery {
 public:
  // Options for S2HausdorffDistanceQuery.
  class Options {
   public:
    Options();

    // The include_interiors flag (true by default) indicates that the distance
    // should be computed not just to polygon boundaries of the source index,
    // but to polygon interiors as well. For example, if target shape A is fully
    // contained inside the source shape B, and include_interiors is set to
    // true, then the directed Hausdorff distance from A to B is going to be
    // zero.
    bool include_interiors() const { return include_interiors_; }

    void set_include_interiors(bool include_interiors) {
      include_interiors_ = include_interiors;
    }

   private:
    bool include_interiors_ = true;
  };

  // DirectedResult stores the results of directed Hausdorff distance queries
  class DirectedResult {
   public:
    // Initializes this instance of DirectedResult with given values.
    DirectedResult(const S1ChordAngle& distance, const S2Point& target_point)
        : distance_(distance), target_point_(target_point) {}

    // Returns the resulting directed Hausdorff distance value.
    S1ChordAngle distance() const { return distance_; }

    // Returns the point on the target index on which the directed Hausdorff
    // distance is achieved.
    const S2Point& target_point() const { return target_point_; }

   private:
    S1ChordAngle distance_;
    S2Point target_point_;
  };

  // Result stores the output of [undirected] Hausdorff distance query. It
  // consists of two directed query results, forward and reverse.
  class Result {
   public:
    // Initializes the Result with the directed result of the forward query
    // (target index to source index) and that of the reverse (source index
    // to target index) query.
    Result(const DirectedResult& target_to_source,
           const DirectedResult& source_to_target)
        : target_to_source_(target_to_source),
          source_to_target_(source_to_target) {}

    // Returns the actual Hausdorff distance, which is the maximum of the two
    // directed query results.
    S1ChordAngle distance() const {
      return std::max(target_to_source_.distance(),
                      source_to_target_.distance());
    }

    // Returns the const reference to the result for the target-to-source
    // directed Hausdorff distance call.
    const DirectedResult& target_to_source() const { return target_to_source_; }

    // Returns the const reference to the result for the source-to-target
    // directed Hausdorff distance call.
    const DirectedResult& source_to_target() const { return source_to_target_; }

   private:
    DirectedResult target_to_source_;
    DirectedResult source_to_target_;
  };

  // This constructor initializes the query with the query options.
  explicit S2HausdorffDistanceQuery(const Options& options = Options());

  // Initializes the query with the options.
  void Init(const Options& options = Options());

  // Accessors for the query options.
  const Options& options() const { return options_; }
  Options* mutable_options() { return &options_; }

  // Compute directed Hausdorff distance from the target index to the source
  // index.  Returns nullopt iff at least one of the shape indexes is empty.
  //
  // Note that directed Hausdorff distance from geometry A (as target) to
  // geometry B (as source) is not (in general case) equal to that from B (as
  // target) to A (as source).
  absl::optional<DirectedResult> GetDirectedResult(
      const S2ShapeIndex* target, const S2ShapeIndex* source) const;

  // Same as the above method, only returns the actual distance, or
  // S1ChordAngle::Infinity() iff at least one of the shape indexes is empty.
  S1ChordAngle GetDirectedDistance(const S2ShapeIndex* target,
                                   const S2ShapeIndex* source) const;

  // Compute the [undirected] Hausdorff distance between the target index
  // and the source index.  Returns nullopt iff at least one of the shape
  // indexes is empty.
  //
  // Note that the result of this query is symmetrical with respect to target
  // vs. source, i.e. if target and source indices are swapped, the
  // resulting Hausdorff distance remains unchanged.
  absl::optional<Result> GetResult(const S2ShapeIndex* target,
                                   const S2ShapeIndex* source) const;

  // Same as the above method, but only returns the maximum of forward and
  // reverse distances, or S1ChordAngle::Infinity() iff at least one of the
  // shape indexes is empty.
  S1ChordAngle GetDistance(const S2ShapeIndex* target,
                           const S2ShapeIndex* source) const;

 private:
  Options options_;
};

//////////////////   Implementation details follow   ////////////////////

inline S2HausdorffDistanceQuery::Options::Options (){
}

#endif  // S2_S2HAUSDORFF_DISTANCE_QUERY_H_
