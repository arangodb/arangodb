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

#include "s2/s2closest_point_query.h"

void S2ClosestPointQueryOptions::set_conservative_max_distance(
    S1ChordAngle max_distance) {
  set_max_distance(Distance(max_distance.PlusError(
      S2::GetUpdateMinDistanceMaxError(max_distance)).Successor()));
}

void S2ClosestPointQueryOptions::set_conservative_max_distance(
    S1Angle max_distance) {
  set_conservative_max_distance(S1ChordAngle(max_distance));
}

int S2ClosestPointQueryPointTarget::max_brute_force_index_size() const {
  // Using BM_FindClosest (which finds the single closest point), the
  // break-even points are approximately X, Y, and Z points for grid,
  // fractal, and regular loop geometry respectively.
  //
  // TODO(ericv): Adjust using benchmarks.
  return 150;
}

int S2ClosestPointQueryEdgeTarget::max_brute_force_index_size() const {
  // Using BM_FindClosestToEdge (which finds the single closest point), the
  // break-even points are approximately X, Y, and Z points for grid,
  // fractal, and regular loop geometry respectively.
  //
  // TODO(ericv): Adjust using benchmarks.
  return 100;
}

int S2ClosestPointQueryCellTarget::max_brute_force_index_size() const {
  // Using BM_FindClosestToCell (which finds the single closest point), the
  // break-even points are approximately X, Y, and Z points for grid,
  // fractal, and regular loop geometry respectively.
  //
  // TODO(ericv): Adjust using benchmarks.
  return 50;
}

int S2ClosestPointQueryShapeIndexTarget::max_brute_force_index_size() const {
  // For BM_FindClosestToSameSizeAbuttingIndex (which uses a nearby
  // S2ShapeIndex target of similar complexity), the break-even points are
  // approximately X, Y, and Z points for grid, fractal, and regular loop
  // geometry respectively.
  //
  // TODO(ericv): Adjust using benchmarks.
  return 30;
}
