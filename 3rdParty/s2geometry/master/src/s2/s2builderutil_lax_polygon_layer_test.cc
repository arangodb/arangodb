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

#include "s2/s2builderutil_lax_polygon_layer.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

#include "s2/base/integral_types.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2debug.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using absl::string_view;
using s2builderutil::IndexedLaxPolygonLayer;
using s2builderutil::LaxPolygonLayer;
using s2textformat::MakeLaxPolygonOrDie;
using s2textformat::MakePointOrDie;
using s2textformat::MakePolylineOrDie;
using std::map;
using std::set;
using std::string;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using DegenerateBoundaries = LaxPolygonLayer::Options::DegenerateBoundaries;

namespace {

string ToString(DegenerateBoundaries degenerate_boundaries) {
  switch (degenerate_boundaries) {
    case DegenerateBoundaries::DISCARD: return "DISCARD";
    case DegenerateBoundaries::DISCARD_HOLES: return "DISCARD_HOLES";
    case DegenerateBoundaries::DISCARD_SHELLS: return "DISCARD_SHELLS";
    case DegenerateBoundaries::KEEP: return "KEEP";
  }
  // Cases are exhaustive, but some compilers don't know that and emit a
  // warning.
  S2_LOG(FATAL) << "Unknown DegenerateBoundaries value: "
                << static_cast<int>(degenerate_boundaries);
}

void TestLaxPolygon(string_view input_str, string_view expected_str,
                    EdgeType edge_type,
                    DegenerateBoundaries degenerate_boundaries) {
  SCOPED_TRACE(edge_type == EdgeType::DIRECTED ? "DIRECTED" : "UNDIRECTED");
  SCOPED_TRACE(ToString(degenerate_boundaries));
  S2Builder builder{S2Builder::Options()};
  S2LaxPolygonShape output;
  LaxPolygonLayer::Options options;
  options.set_edge_type(edge_type);
  options.set_degenerate_boundaries(degenerate_boundaries);
  builder.StartLayer(make_unique<LaxPolygonLayer>(&output, options));

  auto polygon = MakeLaxPolygonOrDie(input_str);
  builder.AddShape(*polygon);

  // In order to construct polygons that are full except possibly for a
  // collection of degenerate holes, we must supply S2Builder with a predicate
  // that distinguishes empty polygons from full ones (modulo degeneracies).
  bool has_full_loop = false;
  for (int i = 0; i < polygon->num_loops(); ++i) {
    if (polygon->num_loop_vertices(i) == 0) has_full_loop = true;
  }
  builder.AddIsFullPolygonPredicate(S2Builder::IsFullPolygon(has_full_loop));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  string actual_str = s2textformat::ToString(output, "; ");
  EXPECT_EQ(expected_str, actual_str);
}

void TestLaxPolygon(string_view input_str,
                    string_view expected_str,
                    DegenerateBoundaries degenerate_boundaries) {
  TestLaxPolygon(input_str, expected_str, EdgeType::DIRECTED,
                 degenerate_boundaries);
#if 0
  // TODO(ericv): Implement.
  TestLaxPolygon(input_str, expected_str, EdgeType::UNDIRECTED,
                 degenerate_boundaries);
#endif
}

void TestLaxPolygonUnchanged(string_view input_str,
                             DegenerateBoundaries degenerate_boundaries) {
  TestLaxPolygon(input_str, input_str, degenerate_boundaries);
}

vector<DegenerateBoundaries> kAllDegenerateBoundaries() {
  return {DegenerateBoundaries::DISCARD, DegenerateBoundaries::DISCARD_HOLES,
          DegenerateBoundaries::DISCARD_SHELLS, DegenerateBoundaries::KEEP};
}

TEST(LaxPolygonLayer, Empty) {
  for (auto db : kAllDegenerateBoundaries()) {
    TestLaxPolygonUnchanged("", db);
  }
}

TEST(LaxPolygonLayer, Full) {
  for (auto db : kAllDegenerateBoundaries()) {
    TestLaxPolygonUnchanged("full", db);
  }
}

TEST(LaxPolygonLayer, OneNormalShell) {
  for (auto db : kAllDegenerateBoundaries()) {
    TestLaxPolygonUnchanged("0:0, 0:1, 1:1", db);
  }
}

TEST(LaxPolygonLayer, IsFullPolygonPredicateNotCalled) {
  // Test that the IsFullPolygonPredicate is not called when at least one
  // non-degenerate loop is present.
  for (auto degenerate_boundaries : kAllDegenerateBoundaries()) {
    S2Builder builder{S2Builder::Options()};
    S2LaxPolygonShape output;
    LaxPolygonLayer::Options options;
    options.set_edge_type(EdgeType::DIRECTED);
    options.set_degenerate_boundaries(degenerate_boundaries);
    builder.StartLayer(make_unique<LaxPolygonLayer>(&output, options));
    auto polygon = MakeLaxPolygonOrDie("0:0, 0:1, 1:1");
    builder.AddShape(*polygon);
    // If the predicate is called, it will return an error.
    builder.AddIsFullPolygonPredicate(S2Builder::IsFullPolygonUnspecified);
    S2Error error;
    ASSERT_TRUE(builder.Build(&error));
  }
}

TEST(LaxPolygonLayer, TwoNormalShellsOneNormalHole) {
  // The second two loops are nested.  Note that S2LaxPolygon and S2Polygon
  // require opposite vertex orderings for holes.
  for (auto db : kAllDegenerateBoundaries()) {
    TestLaxPolygonUnchanged("0:1, 1:1, 0:0; "
                            "3:3, 3:6, 6:6, 6:3; "
                            "4:4, 5:4, 5:5, 4:5", db);
  }
}

TEST(LaxPolygonLayer, AllDegenerateShells) {
  for (auto db : {DegenerateBoundaries::KEEP,
          DegenerateBoundaries::DISCARD_HOLES}) {
    TestLaxPolygonUnchanged("1:1; 2:2, 3:3", db);
  }
  for (auto db : {DegenerateBoundaries::DISCARD,
          DegenerateBoundaries::DISCARD_SHELLS}) {
    TestLaxPolygon("1:1; 2:2, 3:3", "", db);
  }
}

TEST(LaxPolygonLayer, AllDegenerateHoles) {
  for (auto db : {DegenerateBoundaries::KEEP,
          DegenerateBoundaries::DISCARD_SHELLS}) {
    TestLaxPolygonUnchanged("full; 1:1; 2:2, 3:3", db);
  }
  for (auto db : {DegenerateBoundaries::DISCARD,
          DegenerateBoundaries::DISCARD_HOLES}) {
    TestLaxPolygon("full; 1:1; 2:2, 3:3", "full", db);
  }
}

TEST(LaxPolygonLayer, SomeDegenerateShells) {
  const string kNormal = "0:0, 0:9, 9:0; 1:1, 7:1, 1:7";
  const string kInput = kNormal + "; 3:2; 2:2, 2:3";
  TestLaxPolygonUnchanged(kInput, DegenerateBoundaries::KEEP);
  TestLaxPolygonUnchanged(kInput, DegenerateBoundaries::DISCARD_HOLES);
  TestLaxPolygon(kInput, kNormal, DegenerateBoundaries::DISCARD);
  TestLaxPolygon(kInput, kNormal, DegenerateBoundaries::DISCARD_SHELLS);
}

TEST(LaxPolygonLayer, SomeDegenerateHoles) {
  for (auto db : {DegenerateBoundaries::KEEP,
          DegenerateBoundaries::DISCARD_SHELLS}) {
    TestLaxPolygonUnchanged("0:0, 0:9, 9:0; 1:1; 2:2, 3:3", db);
  }
  for (auto db : {DegenerateBoundaries::DISCARD,
          DegenerateBoundaries::DISCARD_HOLES}) {
    TestLaxPolygon("0:0, 0:9, 9:0; 1:1; 2:2, 3:3", "0:0, 0:9, 9:0", db);
  }
}

TEST(LaxPolygonLayer, NormalAndDegenerateShellsAndHoles) {
  // We start with two normal shells and one normal hole.
  const string kNormal = "0:0, 0:9, 9:9, 9:0; "
                         "0:10, 0:19, 9:19, 9:10; 1:11, 8:11, 8:18, 1:18";
  // These are the same loops augmented with degenerate interior filaments
  // (holes).  Note that one filament connects the second shell and hole
  // above, transforming them into a single loop.
  const string kNormalWithDegenHoles =
      "0:0, 0:9, 1:8, 1:7, 1:8, 0:9, 9:9, 9:0; "
      "0:10, 0:19, 9:19, 9:10, 0:10, 1:11, 8:11, 8:18, 1:18, 1:11";
  // Then we add other degenerate shells and holes, including a sibling pair
  // that connects the two shells above.
  const string kDegenShells = "0:9, 0:10; 2:12; 3:13, 3:14; 20:20; 10:0, 10:1";
  const string kDegenHoles = "2:5; 3:6, 3:7; 8:8";
  const string kInput = kNormalWithDegenHoles + "; " +
                        kDegenShells + "; " + kDegenHoles;
  TestLaxPolygon(kInput, kNormal, DegenerateBoundaries::DISCARD);
  TestLaxPolygon(kInput, kNormal + "; " + kDegenShells,
                 DegenerateBoundaries::DISCARD_HOLES);
  TestLaxPolygon(kInput, kNormalWithDegenHoles + "; " + kDegenHoles,
                 DegenerateBoundaries::DISCARD_SHELLS);
  TestLaxPolygon(kInput, kInput, DegenerateBoundaries::KEEP);
}

TEST(LaxPolygonLayer, PartialLoop) {
  S2Builder builder{S2Builder::Options()};
  S2LaxPolygonShape output;
  builder.StartLayer(make_unique<LaxPolygonLayer>(&output));
  builder.AddPolyline(*MakePolylineOrDie("0:1, 2:3, 4:5"));
  S2Error error;
  EXPECT_FALSE(builder.Build(&error));
  EXPECT_EQ(S2Error::BUILDER_EDGES_DO_NOT_FORM_LOOPS, error.code());
  EXPECT_TRUE(output.is_empty());
}

#if 0
// TODO(ericv): Implement validation of S2LaxPolygonShape.
TEST(LaxPolygonLayer, InvalidPolygon) {
  S2Builder builder{S2Builder::Options()};
  S2LaxPolygonShape output;
  LaxPolygonLayer::Options options;
  options.set_validate(true);
  builder.StartLayer(make_unique<LaxPolygonLayer>(&output, options));
  builder.AddPolyline(*MakePolylineOrDie("0:0, 0:10, 10:0, 10:10, 0:0"));
  S2Error error;
  EXPECT_FALSE(builder.Build(&error));
  EXPECT_EQ(S2Error::LOOP_SELF_INTERSECTION, error.code());
}
#endif

TEST(LaxPolygonLayer, DuplicateInputEdges) {
  // Check that LaxPolygonLayer removes duplicate edges in such a way that
  // degeneracies are not lost.
  S2Builder builder{S2Builder::Options()};
  S2LaxPolygonShape output;
  LaxPolygonLayer::Options options;
  options.set_degenerate_boundaries(DegenerateBoundaries::KEEP);
  builder.StartLayer(make_unique<LaxPolygonLayer>(&output, options));
  builder.AddShape(*MakeLaxPolygonOrDie("0:0, 0:5, 5:5, 5:0"));
  builder.AddPoint(MakePointOrDie("0:0"));
  builder.AddPoint(MakePointOrDie("1:1"));
  builder.AddPoint(MakePointOrDie("1:1"));
  builder.AddShape(*MakeLaxPolygonOrDie("2:2, 2:3"));
  builder.AddShape(*MakeLaxPolygonOrDie("2:2, 2:3"));
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  EXPECT_EQ("0:0, 0:5, 5:5, 5:0; 1:1; 2:2, 2:3",
            s2textformat::ToString(output, "; "));
}

using EdgeLabelMap = map<S2Shape::Edge, set<int32>>;

inline S2Shape::Edge GetKey(S2Shape::Edge edge, EdgeType edge_type) {
  // For undirected edges, sort the vertices in lexicographic order.
  if (edge_type == EdgeType::UNDIRECTED && edge.v0 > edge.v1) {
    std::swap(edge.v0, edge.v1);
  }
  return edge;
}

void AddShapeWithLabels(const S2Shape& shape, EdgeType edge_type,
                        S2Builder* builder, EdgeLabelMap *edge_label_map) {
  static int const kLabelBegin = 1234;  // Arbitrary.
  for (int e = 0; e < shape.num_edges(); ++e) {
    int32 label = kLabelBegin + e;
    builder->set_label(label);
    // For undirected edges, reverse the direction of every other input edge.
    S2Shape::Edge edge = shape.edge(e);
    if (edge_type == EdgeType::UNDIRECTED && (e & 1)) {
      std::swap(edge.v0, edge.v1);
    }
    builder->AddEdge(edge.v0, edge.v1);
    (*edge_label_map)[GetKey(edge, edge_type)].insert(label);
  }
}

// Converts "input_str" to an S2LaxPolygonShape, assigns labels to its edges,
// then uses LaxPolygonLayer with the given arguments to build a new
// S2LaxPolygonShape and verifies that all edges have the expected labels.
// (This function does not test whether the output edges are correct.)
static void TestEdgeLabels(string_view input_str, EdgeType edge_type,
                           DegenerateBoundaries degenerate_boundaries) {
  S2Builder builder{S2Builder::Options()};
  S2LaxPolygonShape output;
  LaxPolygonLayer::LabelSetIds label_set_ids;
  IdSetLexicon label_set_lexicon;
  LaxPolygonLayer::Options options;
  options.set_edge_type(edge_type);
  options.set_degenerate_boundaries(degenerate_boundaries);
  builder.StartLayer(make_unique<LaxPolygonLayer>(
      &output, &label_set_ids, &label_set_lexicon, options));

  EdgeLabelMap edge_label_map;
  AddShapeWithLabels(*MakeLaxPolygonOrDie(input_str), edge_type,
                     &builder, &edge_label_map);
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  for (int i = 0; i < output.num_chains(); ++i) {
    for (int j = 0; j < output.chain(i).length; ++j) {
      S2Shape::Edge edge = output.chain_edge(i, j);
      const auto& expected_labels = edge_label_map[GetKey(edge, edge_type)];
      ASSERT_EQ(expected_labels.size(),
                label_set_lexicon.id_set(label_set_ids[i][j]).size());
      EXPECT_TRUE(std::equal(
          expected_labels.begin(), expected_labels.end(),
          label_set_lexicon.id_set(label_set_ids[i][j]).begin()));
    }
  }
}

TEST(LaxPolygonLayer, EdgeLabels) {
  // TODO(ericv): Implement EdgeType::UNDIRECTED.
  for (auto edge_type : {EdgeType::DIRECTED}) {
    for (auto db : kAllDegenerateBoundaries()) {
      // Test a polygon with normal and degenerate shells and holes.  Note
      // that this S2LaxPolygonShape has duplicate edges and is therefore not
      // valid in most contexts.
      TestEdgeLabels("1:1, 1:2; 0:0, 0:9, 9:9, 9:0; 1:2, 1:1; "
                     "3:3, 8:3, 8:8, 3:8; 4:4; 4:5, 5:5; 4:4", edge_type, db);
    }
  }
}

TEST(IndexedLaxPolygonLayer, AddsShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedLaxPolygonLayer>(&index));
  const string& polygon_str = "0:0, 0:10, 10:0";
  builder.AddPolygon(*s2textformat::MakePolygonOrDie(polygon_str));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(1, index.num_shape_ids());
  auto polygon = down_cast<const S2LaxPolygonShape*>(index.shape(0));
  EXPECT_EQ(polygon_str, s2textformat::ToString(*polygon));
}

TEST(IndexedLaxPolygonLayer, IgnoresEmptyShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedLaxPolygonLayer>(&index));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(0, index.num_shape_ids());
}

}  // namespace
