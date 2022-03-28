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

#include "s2/s2shape_measures.h"

#include <cmath>
#include <vector>

#include "s2/base/log_severity.h"
#include "s2/s2loop_measures.h"
#include "s2/s2polyline_measures.h"

using std::fabs;
using std::vector;

namespace S2 {

S1Angle GetLength(const S2Shape& shape) {
  if (shape.dimension() != 1) return S1Angle::Zero();
  S1Angle length;
  vector<S2Point> vertices;
  int num_chains = shape.num_chains();
  for (int chain_id = 0; chain_id < num_chains; ++chain_id) {
    GetChainVertices(shape, chain_id, &vertices);
    length += S2::GetLength(vertices);
  }
  return length;
}

S1Angle GetPerimeter(const S2Shape& shape) {
  if (shape.dimension() != 2) return S1Angle::Zero();
  S1Angle perimeter;
  vector<S2Point> vertices;
  int num_chains = shape.num_chains();
  for (int chain_id = 0; chain_id < num_chains; ++chain_id) {
    GetChainVertices(shape, chain_id, &vertices);
    perimeter += S2::GetPerimeter(S2PointLoopSpan(vertices));
  }
  return perimeter;
}

double GetArea(const S2Shape& shape) {
  if (shape.dimension() != 2) return 0.0;

  // Since S2Shape uses the convention that the interior of the shape is to
  // the left of all edges, in theory we could compute the area of the polygon
  // by simply adding up all the loop areas modulo 4*Pi.  The problem with
  // this approach is that polygons holes typically have areas near 4*Pi,
  // which can create large cancellation errors when computing the area of
  // small polygons with holes.  For example, a shell with an area of 4 square
  // meters (1e-13 steradians) surrounding a hole with an area of 3 square
  // meters (7.5e-14 sterians) would lose almost all of its accuracy if the
  // area of the hole was computed as 12.566370614359098.
  //
  // So instead we use S2::GetSignedArea() to ensure that all loops have areas
  // in the range [-2*Pi, 2*Pi].
  //
  // TODO(ericv): Rarely, this function returns the area of the complementary
  // region (4*Pi - area).  This can only happen when the true area is very
  // close to zero or 4*Pi and the polygon has multiple loops.  To make this
  // function completely robust requires checking whether the signed area sum is
  // ambiguous, and if so, determining the loop nesting structure.  This allows
  // the sum to be evaluated in a way that is guaranteed to have the correct
  // sign.
  double area = 0;
  double max_error = 0;
  vector<S2Point> vertices;
  int num_chains = shape.num_chains();
  for (int chain_id = 0; chain_id < num_chains; ++chain_id) {
    GetChainVertices(shape, chain_id, &vertices);
    area += S2::GetSignedArea(S2PointLoopSpan(vertices));
    if (google::DEBUG_MODE) {
      max_error += S2::GetCurvatureMaxError(S2PointLoopSpan(vertices));
    }
  }
  // Note that S2::GetSignedArea() guarantees that the full loop (containing
  // all points on the sphere) has a very small negative area.
  S2_DCHECK_LE(fabs(area), 4 * M_PI + max_error);
  if (area < 0.0) area += 4 * M_PI;
  return area;
}

double GetApproxArea(const S2Shape& shape) {
  if (shape.dimension() != 2) return 0.0;

  double area = 0;
  vector<S2Point> vertices;
  int num_chains = shape.num_chains();
  for (int chain_id = 0; chain_id < num_chains; ++chain_id) {
    GetChainVertices(shape, chain_id, &vertices);
    area += S2::GetApproxArea(S2PointLoopSpan(vertices));
  }
  // Special case to ensure that full polygons are handled correctly.
  if (area <= 4 * M_PI) return area;
  return fmod(area, 4 * M_PI);
}

S2Point GetCentroid(const S2Shape& shape) {
  S2Point centroid;
  vector<S2Point> vertices;
  int dimension = shape.dimension();
  int num_chains = shape.num_chains();
  for (int chain_id = 0; chain_id < num_chains; ++chain_id) {
    switch (dimension) {
      case 0:
        centroid += shape.edge(chain_id).v0;
        break;
      case 1:
        GetChainVertices(shape, chain_id, &vertices);
        centroid += S2::GetCentroid(S2PointSpan(vertices));
        break;
      default:
        GetChainVertices(shape, chain_id, &vertices);
        centroid += S2::GetCentroid(S2PointLoopSpan(vertices));
        break;
    }
  }
  return centroid;
}

void GetChainVertices(const S2Shape& shape, int chain_id,
                      std::vector<S2Point>* vertices) {
  S2Shape::Chain chain = shape.chain(chain_id);
  int num_vertices = chain.length + (shape.dimension() == 1);
  vertices->clear();
  vertices->reserve(num_vertices);
  int e = 0;
  if (num_vertices & 1) {
    vertices->push_back(shape.chain_edge(chain_id, e++).v0);
  }
  for (; e < num_vertices; e += 2) {
    auto edge = shape.chain_edge(chain_id, e);
    vertices->push_back(edge.v0);
    vertices->push_back(edge.v1);
  }
}

}  // namespace S2
