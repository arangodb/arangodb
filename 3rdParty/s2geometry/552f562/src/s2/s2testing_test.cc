// Copyright 2012 Google Inc. All Rights Reserved.
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

#include "s2/s2testing.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "s2/base/logging.h"
#include <gtest/gtest.h>

#include "s2/s1angle.h"
#include "s2/s2loop.h"

using std::max;
using std::min;
using std::unique_ptr;

namespace {

int NumVerticesAtLevel(int level) {
  S2_DCHECK(level >= 0 && level <= 14);  // Sanity / overflow check
  return 3 * (1 << (2 * level));      // 3*(4**level)
}

void TestFractal(int min_level, int max_level, double dimension) {
  // This function constructs a fractal and then computes various metrics
  // (number of vertices, total length, minimum and maximum radius) and
  // verifies that they are within expected tolerances.  Essentially this
  // directly verifies that the shape constructed *is* a fractal, i.e. the
  // total length of the curve increases exponentially with the level, while
  // the area bounded by the fractal is more or less constant.

  // The radius needs to be fairly small to avoid spherical distortions.
  const double nominal_radius = 0.001;  // radians, or about 6km
  const double kDistortionError = 1e-5;

  S2Testing::Fractal fractal;
  fractal.set_min_level(min_level);
  fractal.set_max_level(max_level);
  fractal.set_fractal_dimension(dimension);
  Matrix3x3_d frame = S2Testing::GetRandomFrame();
  unique_ptr<S2Loop> loop(
      fractal.MakeLoop(frame, S1Angle::Radians(nominal_radius)));
  ASSERT_TRUE(loop->IsValid());

  // If min_level and max_level are not equal, then the number of vertices and
  // the total length of the curve are subject to random variation.  Here we
  // compute an approximation of the standard deviation relative to the mean,
  // noting that most of the variance is due to the random choices about
  // whether to stop subdividing at "min_level" or not.  (The random choices
  // at higher levels contribute progressively less and less to the variance.)
  // The "relative_error" below corresponds to *one* standard deviation of
  // error; it can be increased to a higher multiple if necessary.
  //
  // Details: Let n=3*(4**min_level) and k=(max_level-min_level+1).  Each of
  // the "n" edges at min_level stops subdividing at that level with
  // probability (1/k).  This gives a binomial distribution with mean u=(n/k)
  // and standard deviation s=sqrt((n/k)(1-1/k)).  The relative error (s/u)
  // can be simplified to sqrt((k-1)/n).
  int num_levels = max_level - min_level + 1;
  int min_vertices = NumVerticesAtLevel(min_level);
  double relative_error = sqrt((num_levels - 1.0) / min_vertices);

  // "expansion_factor" is the total fractal length at level "n+1" divided by
  // the total fractal length at level "n".
  double expansion_factor = pow(4, 1 - 1/dimension);
  double expected_num_vertices = 0;
  double expected_length_sum = 0;

  // "triangle_perim" is the perimeter of the original equilateral triangle
  // before any subdivision occurs.
  double triangle_perim = 3 * sqrt(3) * tan(nominal_radius);
  double min_length_sum = triangle_perim * pow(expansion_factor, min_level);
  for (int level = min_level; level <= max_level; ++level) {
    expected_num_vertices += NumVerticesAtLevel(level);
    expected_length_sum += pow(expansion_factor, level);
  }
  expected_num_vertices /= num_levels;
  expected_length_sum *= triangle_perim / num_levels;

  EXPECT_GE(loop->num_vertices(), min_vertices);
  EXPECT_LE(loop->num_vertices(), NumVerticesAtLevel(max_level));
  EXPECT_NEAR(expected_num_vertices, loop->num_vertices(),
              relative_error * (expected_num_vertices - min_vertices));

  S2Point center = frame.Col(2);
  double min_radius = 2 * M_PI;
  double max_radius = 0;
  double length_sum = 0;
  for (int i = 0; i < loop->num_vertices(); ++i) {
    // Measure the radius of the fractal in the tangent plane at "center".
    double r = tan(center.Angle(loop->vertex(i)));
    min_radius = min(min_radius, r);
    max_radius = max(max_radius, r);
    length_sum += loop->vertex(i).Angle(loop->vertex(i+1));
  }
  // kVertexError is an approximate bound on the error when computing vertex
  // positions of the fractal (due to S2::FromFrame, trig calculations, etc).
  const double kVertexError = 1e-14;

  // Although min_radius_factor() is only a lower bound in general, it happens
  // to be exact (to within numerical errors) unless the dimension is in the
  // range (1.0, 1.09).
  if (dimension == 1.0 || dimension >= 1.09) {
    // Expect the min radius to match very closely.
    EXPECT_NEAR(min_radius, fractal.min_radius_factor() * nominal_radius,
                kVertexError);
  } else {
    // Expect the min radius to satisfy the lower bound.
    EXPECT_GE(min_radius,
              fractal.min_radius_factor() * nominal_radius - kVertexError);
  }
  // max_radius_factor() is exact (modulo errors) for all dimensions.
  EXPECT_NEAR(max_radius, fractal.max_radius_factor() * nominal_radius,
              kVertexError);

  EXPECT_NEAR(expected_length_sum, length_sum,
              relative_error * (expected_length_sum - min_length_sum) +
              kDistortionError * length_sum);
}

TEST(S2Testing, TriangleFractal) {
  TestFractal(7, 7, 1.0);
}

TEST(S2Testing, TriangleMultiFractal) {
  TestFractal(2, 6, 1.0);
}

TEST(S2Testing, SpaceFillingFractal) {
  TestFractal(4, 4, 1.999);
}

TEST(S2Testing, KochCurveFractal) {
  TestFractal(7, 7, log(4)/log(3));
}

TEST(S2Testing, KochCurveMultiFractal) {
  TestFractal(4, 8, log(4)/log(3));
}

TEST(S2Testing, CesaroFractal) {
  TestFractal(7, 7, 1.8);
}

TEST(S2Testing, CesaroMultiFractal) {
  TestFractal(3, 6, 1.8);
}

}  // namespace
