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

#include "s2/s2polyline_alignment.h"
#include "s2/s2polyline_alignment_internal.h"

#include <vector>

#include <gtest/gtest.h>

#include "s2/base/stringprintf.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/third_party/absl/strings/str_cat.h"
#include "s2/s2cap.h"
#include "s2/s2loop.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

namespace s2polyline_alignment {

// PRIVATE API TESTS

TEST(S2PolylineAlignmentTest, CreatesWindowFromStrides) {
  //    0 1 2 3 4 5
  //  0 * * * . . .
  //  1 . * * * . .
  //  2 . . * * . .
  //  3 . . . * * *
  //  4 . . . . * *
  const std::vector<ColumnStride> strides = {
      {0, 3}, {1, 4}, {2, 4}, {3, 6}, {4, 6}};
  const Window w(strides);
  EXPECT_EQ(w.GetColumnStride(0).start, 0);
  EXPECT_EQ(w.GetColumnStride(0).end, 3);
  EXPECT_EQ(w.GetColumnStride(4).start, 4);
  EXPECT_EQ(w.GetColumnStride(4).end, 6);
}

TEST(S2PolylineAlignmentTest, CreatesWindowFromWarpPath) {
  //   0 1 2 3 4 5
  // 0 * . . . . .
  // 1 * * . . . .
  // 2 . * . . . .
  // 3 . * * * . .
  // 4 . . . . * *
  const WarpPath path = {{0, 0}, {1, 0}, {1, 1}, {2, 1}, {3, 1},
                         {3, 2}, {3, 3}, {4, 4}, {4, 5}};
  const Window w(path);
  EXPECT_EQ(w.GetColumnStride(0).start, 0);
  EXPECT_EQ(w.GetColumnStride(0).end, 1);
  EXPECT_EQ(w.GetColumnStride(1).start, 0);
  EXPECT_EQ(w.GetColumnStride(1).end, 2);
  EXPECT_EQ(w.GetColumnStride(2).start, 1);
  EXPECT_EQ(w.GetColumnStride(2).end, 2);
  EXPECT_EQ(w.GetColumnStride(3).start, 1);
  EXPECT_EQ(w.GetColumnStride(3).end, 4);
  EXPECT_EQ(w.GetColumnStride(4).start, 4);
  EXPECT_EQ(w.GetColumnStride(4).end, 6);
}

TEST(S2PolylineAlignmentTest, GeneratesWindowDebugString) {
  const std::vector<ColumnStride> strides = {{0, 4}, {0, 4}, {0, 4}, {0, 4}};
  const Window w(strides);
  const string expected_output = R"(
 * * * *
 * * * *
 * * * *
 * * * *
)";
  EXPECT_EQ("\n" + w.DebugString(), expected_output);
}

TEST(S2PolylineAlignmentTest, UpsamplesWindowByFactorOfTwo) {
  //   0 1 2 3 4 5
  // 0 * * * . . .
  // 1 . * * * . .
  // 2 . . * * . .
  // 3 . . . * * *
  // 4 . . . . * *
  const std::vector<ColumnStride> strides = {
      {0, 3}, {1, 4}, {2, 4}, {3, 6}, {4, 6}};
  const Window w(strides);
  const Window w_upscaled = w.Upsample(10, 12);
  const string expected_output = R"(
 * * * * * * . . . . . .
 * * * * * * . . . . . .
 . . * * * * * * . . . .
 . . * * * * * * . . . .
 . . . . * * * * . . . .
 . . . . * * * * . . . .
 . . . . . . * * * * * *
 . . . . . . * * * * * *
 . . . . . . . . * * * *
 . . . . . . . . * * * *
)";
  EXPECT_EQ("\n" + w_upscaled.DebugString(), expected_output);
}

TEST(S2PolylineAlignmentTest, UpsamplesWindowXAxisByFactorOfThree) {
  //   0 1 2 3 4 5
  // 0 * * * . . .
  // 1 . * * * . .
  // 2 . . * * . .  3 . . . * * *
  // 4 . . . . * *
  const std::vector<ColumnStride> strides = {
      {0, 3}, {1, 4}, {2, 4}, {3, 6}, {4, 6}};
  const Window w(strides);
  const Window w_upscaled = w.Upsample(5, 18);
  const string expected_output = R"(
 * * * * * * * * * . . . . . . . . .
 . . . * * * * * * * * * . . . . . .
 . . . . . . * * * * * * . . . . . .
 . . . . . . . . . * * * * * * * * *
 . . . . . . . . . . . . * * * * * *
)";
  EXPECT_EQ("\n" + w_upscaled.DebugString(), expected_output);
}

TEST(S2PolylineAlignmentTest, UpsamplesWindowYAxisByFactorOfThree) {
  //   0 1 2 3 4 5
  // 0 * * * . . .
  // 1 . * * * . .
  // 2 . . * * . .
  // 3 . . . * * *
  // 4 . . . . * *
  const std::vector<ColumnStride> strides = {
      {0, 3}, {1, 4}, {2, 4}, {3, 6}, {4, 6}};
  const Window w(strides);
  const Window w_upscaled = w.Upsample(15, 6);
  const string expected_output = R"(
 * * * . . .
 * * * . . .
 * * * . . .
 . * * * . .
 . * * * . .
 . * * * . .
 . . * * . .
 . . * * . .
 . . * * . .
 . . . * * *
 . . . * * *
 . . . * * *
 . . . . * *
 . . . . * *
 . . . . * *
)";
  EXPECT_EQ("\n" + w_upscaled.DebugString(), expected_output);
}

TEST(S2PolylineAlignmentTest, UpsamplesWindowByNonInteger) {
  //   0 1 2 3 4 5
  // 0 * * * . . .
  // 1 . * * * . .
  // 2 . . * * . .
  // 3 . . . * * *
  // 4 . . . . * *
  const std::vector<ColumnStride> strides = {
      {0, 3}, {1, 4}, {2, 4}, {3, 6}, {4, 6}};
  const Window w(strides);

  const Window w_upscaled = w.Upsample(19, 23);
  const string expected_output = R"(
 * * * * * * * * * * * * . . . . . . . . . . .
 * * * * * * * * * * * * . . . . . . . . . . .
 * * * * * * * * * * * * . . . . . . . . . . .
 * * * * * * * * * * * * . . . . . . . . . . .
 . . . . * * * * * * * * * * * . . . . . . . .
 . . . . * * * * * * * * * * * . . . . . . . .
 . . . . * * * * * * * * * * * . . . . . . . .
 . . . . * * * * * * * * * * * . . . . . . . .
 . . . . . . . . * * * * * * * . . . . . . . .
 . . . . . . . . * * * * * * * . . . . . . . .
 . . . . . . . . * * * * * * * . . . . . . . .
 . . . . . . . . . . . . * * * * * * * * * * *
 . . . . . . . . . . . . * * * * * * * * * * *
 . . . . . . . . . . . . * * * * * * * * * * *
 . . . . . . . . . . . . * * * * * * * * * * *
 . . . . . . . . . . . . . . . * * * * * * * *
 . . . . . . . . . . . . . . . * * * * * * * *
 . . . . . . . . . . . . . . . * * * * * * * *
 . . . . . . . . . . . . . . . * * * * * * * *
)";
  EXPECT_EQ("\n" + w_upscaled.DebugString(), expected_output);
}

TEST(S2PolylineAlignmentTest, DilatesWindowByRadiusZero) {
  //   0 1 2 3 4 5
  // 0 * * * . . .
  // 1 . . * . . .
  // 2 . . * . . .
  // 3 . . * * . .
  // 4 . . . * * *
  const std::vector<ColumnStride> strides = {
      {0, 3}, {2, 3}, {2, 3}, {2, 4}, {3, 6}};
  const Window w(strides);
  const Window w_d = w.Dilate(0);
  const string expected_output = R"(
 * * * . . .
 . . * . . .
 . . * . . .
 . . * * . .
 . . . * * *
)";
  EXPECT_EQ("\n" + w_d.DebugString(), expected_output);
}

TEST(S2PolylineAlignmentTest, DilatesWindowByRadiusOne) {
  //   0 1 2 3 4 5 (x's are the spots that we dilate into)
  // 0 * * * x . .
  // 1 x x * x . .
  // 2 . x * x x .
  // 3 . x * * x x
  // 4 . x x * * *
  const std::vector<ColumnStride> strides = {
      {0, 3}, {2, 3}, {2, 3}, {2, 4}, {3, 6}};
  const Window w(strides);
  const Window w_d = w.Dilate(1);
  const string expected_output = R"(
 * * * * . .
 * * * * . .
 . * * * * .
 . * * * * *
 . * * * * *
)";
  EXPECT_EQ("\n" + w_d.DebugString(), expected_output);
}

TEST(S2PolylineAlignmentTest, DilatesWindowByRadiusTwo) {
  //   0 1 2 3 4 5 (x's are the spots that we dilate into)
  // 0 * * * x x .
  // 1 x x * x x x
  // 2 x x * x x x
  // 3 x x * * x x
  // 4 x x x * * *
  const std::vector<ColumnStride> strides = {
      {0, 3}, {2, 3}, {2, 3}, {2, 4}, {3, 6}};
  const Window w(strides);
  const Window w_d = w.Dilate(2);
  const string expected_output = R"(
 * * * * * .
 * * * * * *
 * * * * * *
 * * * * * *
 * * * * * *
)";
  EXPECT_EQ("\n" + w_d.DebugString(), expected_output);
}
TEST(S2PolylineAlignmentTest, DilatesWindowByVeryLargeRadius) {
  const std::vector<ColumnStride> strides = {
      {0, 3}, {2, 3}, {2, 3}, {2, 4}, {3, 6}};
  const Window w(strides);
  const Window w_d = w.Dilate(100);
  const string expected_output = R"(
 * * * * * *
 * * * * * *
 * * * * * *
 * * * * * *
 * * * * * *
)";
  EXPECT_EQ("\n" + w_d.DebugString(), expected_output);
}

TEST(S2PolylineAlignmentTest, HalvesZeroLengthPolyline) {
  const auto line = s2textformat::MakePolylineOrDie("");
  const auto halved = HalfResolution(*line);
  const auto correct = s2textformat::MakePolylineOrDie("");
  EXPECT_EQ(s2textformat::ToString(*halved), s2textformat::ToString(*correct));
}

TEST(S2PolylineAlignmentTest, HalvesEvenLengthPolyline) {
  const auto line = s2textformat::MakePolylineOrDie("0:0, 0:1, 0:2, 1:2");
  const auto halved = HalfResolution(*line);
  const auto correct = s2textformat::MakePolylineOrDie("0:0, 0:2");
  EXPECT_EQ(s2textformat::ToString(*halved), s2textformat::ToString(*correct));
}

TEST(S2PolylineAlignmentTest, HalvesOddLengthPolyline) {
  const auto line = s2textformat::MakePolylineOrDie("0:0, 0:1, 0:2, 1:2, 3:5");
  const auto halved = HalfResolution(*line);
  const auto correct = s2textformat::MakePolylineOrDie("0:0, 0:2, 3:5");
  EXPECT_EQ(s2textformat::ToString(*halved), s2textformat::ToString(*correct));
}

// PUBLIC API TESTS

CostTable DistanceMatrix(const S2Polyline& a, const S2Polyline& b) {
  const int a_n = a.num_vertices();
  const int b_n = b.num_vertices();
  auto table = CostTable(a_n, std::vector<double>(b_n));
  for (int i = 0; i < a_n; ++i) {
    for (int j = 0; j < b_n; ++j) {
      table[i][j] = (a.vertex(i) - b.vertex(j)).Norm2();
    }
  }
  return table;
}

// Do some testing against random sequences with a brute-force solver.
// Returns the optimal cost of alignment up until vertex i, j.
double GetBruteForceCost(const CostTable& table, const int i, const int j) {
  if (i == 0 && j == 0) {
    return table[0][0];
  } else if (i == 0) {
    return GetBruteForceCost(table, i, j - 1) + table[i][j];
  } else if (j == 0) {
    return GetBruteForceCost(table, i - 1, j) + table[i][j];
  } else {
    return std::min({GetBruteForceCost(table, i - 1, j - 1),
                     GetBruteForceCost(table, i - 1, j),
                     GetBruteForceCost(table, i, j - 1)}) +
           table[i][j];
  }
}

// Use Brute Force solver to verify exact Dynamic Programming solvers.
void VerifyCost(const S2Polyline& a, const S2Polyline& b) {
  const int a_n = a.num_vertices();
  const int b_n = b.num_vertices();
  const double brute_cost =
      GetBruteForceCost(DistanceMatrix(a, b), a_n - 1, b_n - 1);
  const double exact_cost = GetExactVertexAlignmentCost(a, b);
  const VertexAlignment exact_alignment = GetExactVertexAlignment(a, b);
  EXPECT_FLOAT_EQ(brute_cost, exact_cost);
  EXPECT_FLOAT_EQ(brute_cost, exact_alignment.alignment_cost);
}

// Check that the costs are the same between both exact computation methods, and
// that the warp path matches the one given.
void VerifyPath(const S2Polyline& a, const S2Polyline& b, const WarpPath& p) {
  double correct = 0;
  for (const auto& pair : p) {
    correct += (a.vertex(pair.first) - b.vertex(pair.second)).Norm2();
  }
  const double exact_cost = GetExactVertexAlignmentCost(a, b);
  const VertexAlignment exact_alignment = GetExactVertexAlignment(a, b);
  EXPECT_FLOAT_EQ(correct, exact_cost);
  EXPECT_FLOAT_EQ(correct, exact_alignment.alignment_cost);
  EXPECT_EQ(exact_alignment.warp_path.size(), p.size());
  for (int i = 0; i < exact_alignment.warp_path.size(); ++i) {
    EXPECT_EQ(exact_alignment.warp_path[i], p[i]);
  }
}

// Return vector of length `num_polylines` containing correlated random
// polylines with `num_vertices` vertices each.
//
// First, we construct a regularly spaced base loop with `num_vertices`
// vertices. Then, for each of `num_polylines` iterations, we construct an new
// loop by uniformly perturbing each point in the base loop by an amount equal
// to `perturbation` * edge_length in a spherical cap. If `perturbation` is less
// than 0.5, then we can perturb each point in the second loop by up to 0.5 edge
// lengths in any direction, which will leave that point with only one possible
// closest vertex match in the base loop. On the other hand, if `perturbation`
// is greater than 0.5, then each vertex in the additional loop will more than
// one match (approximately 2*perturbation + 1) on average in the base loop. The
// intent of this method is to provide a set of correlated testing lines for
// benchmarks and fuzz tests.
std::vector<std::unique_ptr<S2Polyline>> GenPolylines(
    const int num_polylines, const int num_vertices,
    const double perturbation) {
  const auto kLoopRadius = S1Angle::Radians(0.01);
  const auto edge_length = 2 * M_PI * kLoopRadius / num_vertices;
  const auto perturbation_radius = perturbation * edge_length;
  const auto center = S2Testing::RandomPoint();
  const auto loop =
      S2Testing::MakeRegularPoints(center, kLoopRadius, num_vertices);

  std::vector<std::unique_ptr<S2Polyline>> polylines;
  polylines.reserve(num_polylines);

  for (int i = 0; i < num_polylines; ++i) {
    std::vector<S2Point> pts;
    pts.reserve(num_vertices);
    for (int j = 0; j < num_vertices; ++j) {
      pts.push_back(
          S2Testing::SamplePoint(S2Cap(loop[j], perturbation_radius)));
    }
    polylines.push_back(absl::make_unique<S2Polyline>(pts));
  }
  return polylines;
}

TEST(S2PolylineAlignmentTest, ExactLengthZeroInputs) {
  const auto a = s2textformat::MakePolylineOrDie("");
  const auto b = s2textformat::MakePolylineOrDie("");
  const WarpPath correct_path = {};
  EXPECT_DEATH(VerifyPath(*a, *b, correct_path), "");
}

TEST(S2PolylineAlignmentTest, ExactLengthZeroInputA) {
  const auto a = s2textformat::MakePolylineOrDie("");
  const auto b = s2textformat::MakePolylineOrDie("0:0, 1:1, 2:2");
  const WarpPath correct_path = {};
  EXPECT_DEATH(VerifyPath(*a, *b, correct_path), "");
}

TEST(S2PolylineAlignmentTest, ExactLengthZeroInputB) {
  const auto a = s2textformat::MakePolylineOrDie("0:0, 1:1, 2:2");
  const auto b = s2textformat::MakePolylineOrDie("");
  const WarpPath correct_path = {};
  EXPECT_DEATH(VerifyPath(*a, *b, correct_path), "");
}

TEST(S2PolylineAlignmentTest, ExactLengthOneInputs) {
  const auto a = s2textformat::MakePolylineOrDie("1:1");
  const auto b = s2textformat::MakePolylineOrDie("2:2");
  const WarpPath correct_path = {{0, 0}};
  VerifyPath(*a, *b, correct_path);
  VerifyCost(*a, *b);
}

TEST(S2PolylineAlignmentTest, ExactLengthOneInputA) {
  const auto a = s2textformat::MakePolylineOrDie("0:0");
  const auto b = s2textformat::MakePolylineOrDie("0:0, 1:1, 2:2");
  const WarpPath correct_path = {{0, 0}, {0, 1}, {0, 2}};
  VerifyPath(*a, *b, correct_path);
  VerifyCost(*a, *b);
}

TEST(S2PolylineAlignmentTest, ExactLengthOneInputB) {
  const auto a = s2textformat::MakePolylineOrDie("0:0, 1:1, 2:2");
  const auto b = s2textformat::MakePolylineOrDie("0:0");
  const WarpPath correct_path = {{0, 0}, {1, 0}, {2, 0}};
  VerifyPath(*a, *b, correct_path);
  VerifyCost(*a, *b);
}

TEST(S2PolylineAlignmentTest, ExactHeaderFileExample) {
  const auto a = s2textformat::MakePolylineOrDie("1:0, 5:0, 6:0, 9:0");
  const auto b = s2textformat::MakePolylineOrDie("2:0, 7:0, 8:0");
  const WarpPath correct_path = {{0, 0}, {1, 1}, {2, 1}, {3, 2}};
  VerifyPath(*a, *b, correct_path);
  VerifyCost(*a, *b);
}

// Take a small random selection of short correlated polylines and ensure that
// the cost from the brute force solver equals the cost from the DP solvers.
TEST(S2PolylineAlignmentTest, FuzzedWithBruteForce) {
  const int kNumPolylines = 10;
  const int kNumVertices = 8;
  const double kPerturbation = 1.5;
  const auto lines = GenPolylines(kNumPolylines, kNumVertices, kPerturbation);
  for (int i = 0; i < kNumPolylines; ++i) {
    for (int j = i + 1; j < kNumPolylines; ++j) {
      VerifyCost(*lines[i], *lines[j]);
    }
  }
}
}  // namespace s2polyline_alignment
