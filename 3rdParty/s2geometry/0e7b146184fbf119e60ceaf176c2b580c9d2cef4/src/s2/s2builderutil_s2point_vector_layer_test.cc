// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "s2/s2builderutil_s2point_vector_layer.h"

#include <memory>
#include <string>
#include "s2/base/casts.h"
#include "s2/base/integral_types.h"
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2builderutil::IndexedS2PointVectorLayer;
using s2builderutil::S2PointVectorLayer;
using s2textformat::MakePointOrDie;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;

namespace {

void VerifyS2PointVectorLayerResults(
    const S2PointVectorLayer::LabelSetIds& label_set_ids,
    const IdSetLexicon& label_set_lexicon, const vector<S2Point>& output,
    const string& str_expected_points,
    const vector<vector<int32>>& expected_labels) {
  vector<S2Point> expected_points =
      s2textformat::ParsePoints(str_expected_points);

  ASSERT_EQ(expected_labels.size(), label_set_ids.size());
  for (int i = 0; i < output.size(); ++i) {
    EXPECT_EQ(expected_points[i], output[i]);
    ASSERT_EQ(expected_labels[i].size(),
              label_set_lexicon.id_set(label_set_ids[i]).size());
    int k = 0;
    for (int32 label : label_set_lexicon.id_set(label_set_ids[i])) {
      EXPECT_EQ(expected_labels[i][k++], label);
    }
  }
}

void AddPoint(S2Point p, S2Builder* builder) { builder->AddEdge(p, p); }

TEST(S2PointVectorLayer, MergeDuplicates) {
  S2Builder builder{S2Builder::Options()};
  vector<S2Point> output;
  IdSetLexicon label_set_lexicon;
  S2PointVectorLayer::LabelSetIds label_set_ids;
  builder.StartLayer(make_unique<S2PointVectorLayer>(
      &output, &label_set_ids, &label_set_lexicon,
      S2PointVectorLayer::Options(
          S2Builder::GraphOptions::DuplicateEdges::MERGE)));

  builder.set_label(1);
  AddPoint(MakePointOrDie("0:1"), &builder);
  AddPoint(MakePointOrDie("0:2"), &builder);
  builder.set_label(2);
  AddPoint(MakePointOrDie("0:1"), &builder);
  AddPoint(MakePointOrDie("0:4"), &builder);
  AddPoint(MakePointOrDie("0:5"), &builder);
  builder.clear_labels();
  AddPoint(MakePointOrDie("0:5"), &builder);
  AddPoint(MakePointOrDie("0:6"), &builder);
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));

  vector<vector<int32>> expected_labels = {{1, 2}, {1}, {2}, {2}, {}};
  string expected_points = "0:1, 0:2, 0:4, 0:5, 0:6";

  VerifyS2PointVectorLayerResults(label_set_ids, label_set_lexicon, output,
                                  expected_points, expected_labels);
}

TEST(S2PointVectorLayer, KeepDuplicates) {
  S2Builder builder{S2Builder::Options()};
  vector<S2Point> output;
  IdSetLexicon label_set_lexicon;
  S2PointVectorLayer::LabelSetIds label_set_ids;
  builder.StartLayer(make_unique<S2PointVectorLayer>(
      &output, &label_set_ids, &label_set_lexicon,
      S2PointVectorLayer::Options(
          S2Builder::GraphOptions::DuplicateEdges::KEEP)));

  builder.set_label(1);
  AddPoint(MakePointOrDie("0:1"), &builder);
  AddPoint(MakePointOrDie("0:2"), &builder);
  builder.set_label(2);
  AddPoint(MakePointOrDie("0:1"), &builder);
  AddPoint(MakePointOrDie("0:4"), &builder);
  AddPoint(MakePointOrDie("0:5"), &builder);
  builder.clear_labels();
  AddPoint(MakePointOrDie("0:5"), &builder);
  AddPoint(MakePointOrDie("0:6"), &builder);
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));

  vector<vector<int32>> expected_labels = {{1}, {2}, {1}, {2}, {2}, {}, {}};
  string expected_points = "0:1, 0:1, 0:2, 0:4, 0:5, 0:5, 0:6";

  VerifyS2PointVectorLayerResults(label_set_ids, label_set_lexicon, output,
                                  expected_points, expected_labels);
}

TEST(S2PointVectorLayer, Error) {
  S2Builder builder{S2Builder::Options()};
  vector<S2Point> output;
  builder.StartLayer(make_unique<S2PointVectorLayer>(
      &output, S2PointVectorLayer::Options(
                   S2Builder::GraphOptions::DuplicateEdges::KEEP)));

  AddPoint(MakePointOrDie("0:1"), &builder);
  builder.AddEdge(MakePointOrDie("0:3"), MakePointOrDie("0:4"));
  AddPoint(MakePointOrDie("0:5"), &builder);
  S2Error error;
  EXPECT_FALSE(builder.Build(&error));
  EXPECT_EQ(error.code(), S2Error::INVALID_ARGUMENT);
  EXPECT_EQ(error.text(), "Found non-degenerate edges");

  EXPECT_EQ(2, output.size());
  EXPECT_EQ(MakePointOrDie("0:1"), output[0]);
  EXPECT_EQ(MakePointOrDie("0:5"), output[1]);
}

TEST(IndexedS2PointVectorLayer, AddsShapes) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedS2PointVectorLayer>(&index));
  string point0_str = "0:0";
  string point1_str = "2:2";
  builder.AddPoint(s2textformat::MakePointOrDie(point0_str));
  builder.AddPoint(s2textformat::MakePointOrDie(point1_str));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(1, index.num_shape_ids());
  auto shape = down_cast<S2PointVectorShape*>(index.shape(0));
  EXPECT_EQ(2, shape->num_points());
  EXPECT_EQ(point0_str, s2textformat::ToString(shape->point(0)));
  EXPECT_EQ(point1_str, s2textformat::ToString(shape->point(1)));
}

TEST(IndexedS2PointVectorLayer, AddsEmptyShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedS2PointVectorLayer>(&index));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(0, index.num_shape_ids());
}

}  // namespace
