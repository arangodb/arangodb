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

#include "s2/s2shapeutil_visit_crossing_edge_pairs.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2edge_vector_shape.h"
#include "s2/s2shapeutil_contains_brute_force.h"
#include "s2/s2shapeutil_edge_iterator.h"

using absl::make_unique;
using std::vector;

namespace s2shapeutil {

// A set of edge pairs within an S2ShapeIndex.
using EdgePairVector = std::vector<std::pair<ShapeEdgeId, ShapeEdgeId>>;

EdgePairVector GetCrossings(const S2ShapeIndex& index, CrossingType type) {
  EdgePairVector edge_pairs;
  VisitCrossingEdgePairs(
      index, type, [&edge_pairs](const ShapeEdge& a, const ShapeEdge& b, bool) {
        edge_pairs.push_back(std::make_pair(a.id(), b.id()));
        return true;  // Continue visiting.
      });
  if (edge_pairs.size() > 1) {
    std::sort(edge_pairs.begin(), edge_pairs.end());
    edge_pairs.erase(std::unique(edge_pairs.begin(), edge_pairs.end()),
                     edge_pairs.end());
  }
  return edge_pairs;
}

EdgePairVector GetCrossingEdgePairsBruteForce(const S2ShapeIndex& index,
                                              CrossingType type) {
  EdgePairVector result;
  int min_sign = (type == CrossingType::ALL) ? 0 : 1;
  for (EdgeIterator a_iter(&index); !a_iter.Done(); a_iter.Next()) {
    auto a = a_iter.edge();
    EdgeIterator b_iter = a_iter;
    for (b_iter.Next(); !b_iter.Done(); b_iter.Next()) {
      auto b = b_iter.edge();
      if (S2::CrossingSign(a.v0, a.v1, b.v0, b.v1) >= min_sign) {
        result.push_back(
            std::make_pair(a_iter.shape_edge_id(), b_iter.shape_edge_id()));
      }
    }
  }
  return result;
}

std::ostream& operator<<(std::ostream& os,
                         const std::pair<ShapeEdgeId, ShapeEdgeId>& pair) {
  return os << "(" << pair.first << "," << pair.second << ")";
}

void TestGetCrossingEdgePairs(const S2ShapeIndex& index,
                              CrossingType type) {
  EdgePairVector expected = GetCrossingEdgePairsBruteForce(index, type);
  EdgePairVector actual = GetCrossings(index, type);
  if (actual != expected) {
    ADD_FAILURE() << "Unexpected edge pairs; see details below."
                  << "\nExpected number of edge pairs: " << expected.size()
                  << "\nActual number of edge pairs: " << actual.size();
    for (const auto& edge_pair : expected) {
      if (std::count(actual.begin(), actual.end(), edge_pair) != 1) {
        std::cout << "Missing value: " << edge_pair << std::endl;
      }
    }
    for (const auto& edge_pair : actual) {
      if (std::count(expected.begin(), expected.end(), edge_pair) != 1) {
        std::cout << "Extra value: " << edge_pair << std::endl;
      }
    }
  }
}

TEST(GetCrossingEdgePairs, NoIntersections) {
  MutableS2ShapeIndex index;
  TestGetCrossingEdgePairs(index, CrossingType::ALL);
  TestGetCrossingEdgePairs(index, CrossingType::INTERIOR);
}

TEST(GetCrossingEdgePairs, EdgeGrid) {
  const int kGridSize = 10;  // (kGridSize + 1) * (kGridSize + 1) crossings
  MutableS2ShapeIndex index;
  auto shape = make_unique<S2EdgeVectorShape>();
  for (int i = 0; i <= kGridSize; ++i) {
    shape->Add(S2LatLng::FromDegrees(0, i).ToPoint(),
              S2LatLng::FromDegrees(kGridSize, i).ToPoint());
    shape->Add(S2LatLng::FromDegrees(i, 0).ToPoint(),
              S2LatLng::FromDegrees(i, kGridSize).ToPoint());
  }
  index.Add(std::move(shape));
  TestGetCrossingEdgePairs(index, CrossingType::ALL);
  TestGetCrossingEdgePairs(index, CrossingType::INTERIOR);
}

}  // namespace s2shapeutil
