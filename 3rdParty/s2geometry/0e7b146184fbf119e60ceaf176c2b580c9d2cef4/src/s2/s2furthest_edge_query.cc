// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "s2/s2furthest_edge_query.h"

#include <vector>

#include "s2/s2edge_distances.h"

void S2FurthestEdgeQuery::Options::set_conservative_min_distance(
    S1ChordAngle min_distance) {
  set_max_distance(Distance(min_distance.PlusError(
      -S2::GetUpdateMinDistanceMaxError(min_distance)).Predecessor()));
}

void S2FurthestEdgeQuery::Options::set_conservative_min_distance(
    S1Angle min_distance) {
  set_conservative_min_distance(S1ChordAngle(min_distance));
}

// See s2closest_edge_query.cc for justifications of
// max_brute_force_index_size() for that query.
int S2FurthestEdgeQuery::PointTarget::max_brute_force_index_size() const {
  // Using BM_FindFurthest (which finds the single furthest edge), the
  // break-even points are approximately 100, 400, and 600 edges for point
  // cloud, fractal, and regular loop geometry respectively.
  return 300;
}

int S2FurthestEdgeQuery::EdgeTarget::max_brute_force_index_size() const {
  // Using BM_FindFurthestToEdge (which finds the single furthest edge), the
  // break-even points are approximately 80, 100, and 230 edges for point
  // cloud, fractal, and regular loop geometry respectively.
  return 110;
}

int S2FurthestEdgeQuery::CellTarget::max_brute_force_index_size() const {
  // Using BM_FindFurthestToCell (which finds the single furthest edge), the
  // break-even points are approximately 70, 100, and 170 edges for point
  // cloud, fractal, and regular loop geometry respectively.
  return 100;
}

int S2FurthestEdgeQuery::ShapeIndexTarget::max_brute_force_index_size() const {
  // For BM_FindFurthestToSameSizeAbuttingIndex (which uses two nearby indexes
  // with similar edge counts), the break-even points are approximately 30,
  // 100, and 130 edges for point cloud, fractal, and regular loop geometry
  // respectively.
  return 70;
}

S2FurthestEdgeQuery::S2FurthestEdgeQuery() {
  // Prevent inline constructor bloat by defining here.
}

S2FurthestEdgeQuery::~S2FurthestEdgeQuery() {
  // Prevent inline destructor bloat by defining here.
}

void S2FurthestEdgeQuery::FindFurthestEdges(
    Target* target, std::vector<S2FurthestEdgeQuery::Result>* results) {
  results->clear();
  for (auto result : base_.FindClosestEdges(target, options_)) {
    results->push_back(S2FurthestEdgeQuery::Result(result));
  }
}

S2FurthestEdgeQuery::Result S2FurthestEdgeQuery::FindFurthestEdge(
    Target* target) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  Base::Result base_result = base_.FindClosestEdge(target, tmp_options);
  return S2FurthestEdgeQuery::Result(base_result);
}

bool S2FurthestEdgeQuery::IsDistanceGreater(
    Target* target, S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_min_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return base_.FindClosestEdge(target, tmp_options).shape_id() >= 0;
}

bool S2FurthestEdgeQuery::IsDistanceGreaterOrEqual(
    Target* target, S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_inclusive_min_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return base_.FindClosestEdge(target, tmp_options).shape_id() >= 0;
}

bool S2FurthestEdgeQuery::IsConservativeDistanceGreaterOrEqual(
    Target* target, S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_conservative_min_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return base_.FindClosestEdge(target, tmp_options).shape_id() >= 0;
}
