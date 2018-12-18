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

// Author: ericv@google.com (Eric Veach)

#include "s2/s2closest_cell_query.h"

#include <memory>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2edge_distances.h"
#include "s2/s2region_coverer.h"
#include "s2/s2shape_index_region.h"

void S2ClosestCellQuery::Options::set_conservative_max_distance(
    S1ChordAngle max_distance) {
  set_max_distance(Distance(max_distance.PlusError(
      S2::GetUpdateMinDistanceMaxError(max_distance)).Successor()));
}

void S2ClosestCellQuery::Options::set_conservative_max_distance(
    S1Angle max_distance) {
  set_conservative_max_distance(S1ChordAngle(max_distance));
}

// The thresholds for using the brute force algorithm are generally tuned to
// optimize IsDistanceLess (which compares the distance against a threshold)
// rather than FindClosest (which actually computes the minimum distance).
// This is because the former operation is (1) more common, (2) inherently
// faster, and (3) closely related to finding all cells within a given
// distance, which is also very common.

int S2ClosestCellQuery::PointTarget::max_brute_force_index_size() const {
  // Break-even points:                   Point cloud      Cap coverings
  // BM_FindClosest                                18                 16
  // BM_IsDistanceLess                              8                  9
  return 9;
}

int S2ClosestCellQuery::EdgeTarget::max_brute_force_index_size() const {
  // Break-even points:                   Point cloud      Cap coverings
  // BM_FindClosestToLongEdge                      14                 16
  // BM_IsDistanceLessToLongEdge                    5                  5
  return 5;
}

int S2ClosestCellQuery::CellTarget::max_brute_force_index_size() const {
  // Break-even points:                   Point cloud      Cap coverings
  // BM_FindClosestToSmallCell                     12                 13
  // BM_IsDistanceLessToSmallCell                   6                  6
  //
  // Note that the primary use of CellTarget is to implement CellUnionTarget,
  // and therefore it is very important to optimize for the case where a
  // distance limit has been specified.
  return 6;
}

int S2ClosestCellQuery::CellUnionTarget::max_brute_force_index_size() const {
  // Break-even points:                   Point cloud      Cap coverings
  // BM_FindClosestToSmallCoarseCellUnion          12                 10
  // BM_IsDistanceLessToSmallCoarseCellUnion        7                  6
  return 8;
}

int S2ClosestCellQuery::ShapeIndexTarget::max_brute_force_index_size() const {
  // Break-even points:                   Point cloud      Cap coverings
  // BM_FindClosestToSmallCoarseShapeIndex         10                  8
  // BM_IsDistanceLessToSmallCoarseShapeIndex       7                  6
  return 7;
}

S2ClosestCellQuery::S2ClosestCellQuery() {
  // Prevent inline constructor bloat by defining here.
}

S2ClosestCellQuery::~S2ClosestCellQuery() {
  // Prevent inline destructor bloat by defining here.
}

bool S2ClosestCellQuery::IsDistanceLess(Target* target, S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_max_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return !base_.FindClosestCell(target, tmp_options).is_empty();
}

bool S2ClosestCellQuery::IsDistanceLessOrEqual(Target* target,
                                               S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_inclusive_max_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return !base_.FindClosestCell(target, tmp_options).is_empty();
}

bool S2ClosestCellQuery::IsConservativeDistanceLessOrEqual(
    Target* target, S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_conservative_max_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return !base_.FindClosestCell(target, tmp_options).is_empty();
}
