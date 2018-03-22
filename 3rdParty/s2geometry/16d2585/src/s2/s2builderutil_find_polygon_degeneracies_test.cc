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

#include "s2/s2builderutil_find_polygon_degeneracies.h"

#include <iosfwd>
#include <memory>
#include <vector>

#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2cap.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"

using absl::make_unique;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

namespace s2builderutil {

struct TestDegeneracy {
  string edge_str;
  bool is_hole;
  TestDegeneracy(string _edge_str, bool _is_hole)
      : edge_str(_edge_str), is_hole(_is_hole) {
  }
};

bool operator<(const TestDegeneracy& x, const TestDegeneracy& y) {
  if (x.edge_str < y.edge_str) return true;
  if (y.edge_str < x.edge_str) return false;
  return x.is_hole < y.is_hole;
}

bool operator==(const TestDegeneracy& x, const TestDegeneracy& y) {
  return x.edge_str == y.edge_str && x.is_hole == y.is_hole;
}

class DegeneracyCheckingLayer : public S2Builder::Layer {
 public:
  explicit DegeneracyCheckingLayer(const vector<TestDegeneracy>& expected)
      : expected_(expected) {
  }
  GraphOptions graph_options() const override {
    return GraphOptions(EdgeType::DIRECTED, DegenerateEdges::DISCARD_EXCESS,
                        DuplicateEdges::KEEP, SiblingPairs::DISCARD_EXCESS);
  }
  void Build(const Graph& g, S2Error* error) override;

 private:
  vector<TestDegeneracy> expected_;
};

std::ostream& operator<<(std::ostream& os, const vector<TestDegeneracy>& v) {
  for (const auto& degeneracy : v) {
    os << (degeneracy.is_hole ? "Hole(" : "Shell(");
    os << degeneracy.edge_str + ") ";
  }
  return os;
}

void DegeneracyCheckingLayer::Build(const Graph& g, S2Error* error) {
  auto degeneracies = FindPolygonDegeneracies(g, error);
  // Convert the output into a human-readable format.
  vector<TestDegeneracy> actual;
  for (const auto& degeneracy : degeneracies) {
    Graph::Edge edge = g.edge(degeneracy.edge_id);
    vector<S2Point> points { g.vertex(edge.first), g.vertex(edge.second) };
    actual.push_back(TestDegeneracy(s2textformat::ToString(points),
                                    degeneracy.is_hole));
  }
  std::sort(actual.begin(), actual.end());
  std::sort(expected_.begin(), expected_.end());
  EXPECT_TRUE(expected_ == actual) << "\nExpected: " << expected_
                                   << "\nActual: " << actual;
}

void ExpectDegeneracies(const string& polygon_str,
                        const vector<TestDegeneracy>& expected) {
  S2Builder builder{S2Builder::Options()};
  builder.StartLayer(make_unique<DegeneracyCheckingLayer>(expected));
  auto polygon = s2textformat::MakeLaxPolygonOrDie(polygon_str);
  builder.AddIsFullPolygonPredicate(
      [&polygon](const Graph& graph, S2Error* error) {
        return polygon->GetReferencePoint().contained;
      });
  for (int i = 0; i < polygon->num_edges(); ++i) {
    auto edge = polygon->edge(i);
    builder.AddEdge(edge.v0, edge.v1);
  }
  S2Error error;
  EXPECT_TRUE(builder.Build(&error)) << error;
}

TEST(FindPolygonDegeneracies, EmptyPolygon) {
  ExpectDegeneracies("", {});
}

TEST(FindPolygonDegeneracies, NoDegeneracies) {
  ExpectDegeneracies("0:0, 0:1, 1:0", {});
}

TEST(FindPolygonDegeneracies, PointShell) {
  ExpectDegeneracies("0:0", {{"0:0, 0:0", false}});
}

TEST(FindPolygonDegeneracies, SiblingPairShells) {
  ExpectDegeneracies("0:0, 0:1, 1:0; 1:0, 0:1, 0:0", {
      {"0:0, 0:1", false}, {"0:1, 0:0", false}, {"0:1, 1:0", false},
      {"1:0, 0:1", false}, {"0:0, 1:0", false}, {"1:0, 0:0", false}
    });
}

TEST(FindPolygonDegeneracies, AttachedSiblingPairShells) {
  ExpectDegeneracies("0:0, 0:1, 1:0; 1:0, 2:0",
                     {{"1:0, 2:0", false}, {"2:0, 1:0", false}});
}

TEST(FindPolygonDegeneracies, AttachedSiblingPairHoles) {
  ExpectDegeneracies("0:0, 0:3, 3:0; 0:0, 1:1",
                     {{"0:0, 1:1", true}, {"1:1, 0:0", true}});
}

TEST(FindPolygonDegeneracies, AttachedSiblingPairShellsAndHoles) {
  ExpectDegeneracies("0:0, 0:3, 3:0; 3:0, 1:1; 3:0, 5:5", {
      {"3:0, 1:1", true}, {"1:1, 3:0", true},
      {"3:0, 5:5", false}, {"5:5, 3:0", false},
    });
}

TEST(FindPolygonDegeneracies, DegenerateShellsOutsideLoop) {
  ExpectDegeneracies("0:0, 0:3, 3:3, 3:0; 4:4, 5:5; 6:6", {
      {"4:4, 5:5", false}, {"5:5, 4:4", false}, {"6:6, 6:6", false}
    });
}

TEST(FindPolygonDegeneracies, DegenerateHolesWithinLoop) {
  ExpectDegeneracies("0:0, 0:5, 5:5, 5:0; 1:1, 2:2; 3:3", {
      {"1:1, 2:2", true}, {"2:2, 1:1", true}, {"3:3, 3:3", true}
    });
}

TEST(FindPolygonDegeneracies, PointHoleWithinFull) {
  ExpectDegeneracies("full; 0:0", {{"0:0, 0:0", true}});
}

TEST(FindPolygonDegeneracies, SiblingPairHolesWithinFull) {
  ExpectDegeneracies("full; 0:0, 0:1, 1:0; 1:0, 0:1, 0:0", {
      {"0:0, 0:1", true}, {"0:1, 0:0", true}, {"0:1, 1:0", true},
      {"1:0, 0:1", true}, {"0:0, 1:0", true}, {"1:0, 0:0", true}
    });
}

}  // namespace s2builderutil
