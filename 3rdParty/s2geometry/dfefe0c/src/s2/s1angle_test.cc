// Copyright 2005 Google Inc. All Rights Reserved.
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

#include "s2/s1angle.h"

#include <gtest/gtest.h>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/s2latlng.h"
#include "s2/s2testing.h"

TEST(S1Angle, DefaultConstructor) {
  // Check that the default constructor returns an angle of 0.
  S1Angle a;
  EXPECT_EQ(0, a.radians());
}

TEST(S1Angle, Infinity) {
  EXPECT_LT(S1Angle::Radians(1e30), S1Angle::Infinity());
  EXPECT_LT(-S1Angle::Infinity(), S1Angle::Zero());
  EXPECT_EQ(S1Angle::Infinity(), S1Angle::Infinity());
}

TEST(S1Angle, Zero) {
  EXPECT_EQ(S1Angle::Radians(0), S1Angle::Zero());
}

TEST(S1Angle, PiRadiansExactly180Degrees) {
  // Check that the conversion between Pi radians and 180 degrees is exact.
  EXPECT_EQ(M_PI, S1Angle::Radians(M_PI).radians());
  EXPECT_EQ(180.0, S1Angle::Radians(M_PI).degrees());
  EXPECT_EQ(M_PI, S1Angle::Degrees(180).radians());
  EXPECT_EQ(180.0, S1Angle::Degrees(180).degrees());

  EXPECT_EQ(90.0, S1Angle::Radians(M_PI_2).degrees());

  // Check negative angles.
  EXPECT_EQ(-90.0, S1Angle::Radians(-M_PI_2).degrees());
  EXPECT_EQ(-M_PI_4, S1Angle::Degrees(-45).radians());
}

TEST(S1Angle, E5E6E7Representations) {
  // Check that E5/E6/E7 representations work as expected.
  EXPECT_DOUBLE_EQ(S1Angle::Degrees(-45).radians(),
                   S1Angle::E5(-4500000).radians());
  EXPECT_DOUBLE_EQ(S1Angle::Degrees(-60).radians(),
                   S1Angle::E6(-60000000).radians());
  EXPECT_DOUBLE_EQ(S1Angle::Degrees(75).radians(),
                   S1Angle::E7(750000000).radians());
  EXPECT_EQ(-17256123, S1Angle::Degrees(-172.56123).e5());
  EXPECT_EQ(12345678, S1Angle::Degrees(12.345678).e6());
  EXPECT_EQ(-123456789, S1Angle::Degrees(-12.3456789).e7());
}

TEST(S1Angle, E6E7RepresentationsUnsigned) {
  // Check that unsigned E6/E7 representations work as expected.
  EXPECT_DOUBLE_EQ(
      S1Angle::Degrees(60).radians(),
      S1Angle::UnsignedE6(static_cast<uint32>(60000000)).radians());
  EXPECT_DOUBLE_EQ(
      S1Angle::Degrees(-60).radians(),
      S1Angle::UnsignedE6(static_cast<uint32>(-60000000)).radians());
  EXPECT_DOUBLE_EQ(
      S1Angle::Degrees(75).radians(),
      S1Angle::UnsignedE7(static_cast<uint32>(750000000)).radians());
  EXPECT_DOUBLE_EQ(
      S1Angle::Degrees(-75).radians(),
      S1Angle::UnsignedE7(static_cast<uint32>(-750000000)).radians());
}

TEST(S1Angle, NormalizeCorrectlyCanonicalizesAngles) {
  EXPECT_DOUBLE_EQ(0.0, S1Angle::Degrees(360.0).Normalized().degrees());
  EXPECT_DOUBLE_EQ(-90.0, S1Angle::Degrees(-90.0).Normalized().degrees());
  EXPECT_DOUBLE_EQ(180.0, S1Angle::Degrees(-180.0).Normalized().degrees());
  EXPECT_DOUBLE_EQ(180.0, S1Angle::Degrees(180.0).Normalized().degrees());
  EXPECT_DOUBLE_EQ(180.0, S1Angle::Degrees(540.0).Normalized().degrees());
  EXPECT_DOUBLE_EQ(90.0, S1Angle::Degrees(-270.0).Normalized().degrees());
}

TEST(S1Angle, ArithmeticOperationsOnAngles) {
  EXPECT_DOUBLE_EQ(0.3, S1Angle::Radians(-0.3).abs().radians());
  EXPECT_DOUBLE_EQ(-0.1, (-S1Angle::Radians(0.1)).radians());
  EXPECT_DOUBLE_EQ(0.4,
                   (S1Angle::Radians(0.1) + S1Angle::Radians(0.3)).radians());
  EXPECT_DOUBLE_EQ(-0.2,
                   (S1Angle::Radians(0.1) - S1Angle::Radians(0.3)).radians());
  EXPECT_DOUBLE_EQ(0.6, (2 * S1Angle::Radians(0.3)).radians());
  EXPECT_DOUBLE_EQ(0.6, (S1Angle::Radians(0.3) * 2).radians());
  EXPECT_DOUBLE_EQ(0.15, (S1Angle::Radians(0.3) / 2).radians());
  EXPECT_DOUBLE_EQ(0.5, (S1Angle::Radians(0.3) / S1Angle::Radians(0.6)));

  S1Angle tmp = S1Angle::Radians(1.0);
  tmp += S1Angle::Radians(0.5);
  EXPECT_DOUBLE_EQ(1.5, tmp.radians());
  tmp -= S1Angle::Radians(1.0);
  EXPECT_DOUBLE_EQ(0.5, tmp.radians());
  tmp *= 5;
  EXPECT_DOUBLE_EQ(2.5, tmp.radians());
  tmp /= 2;
  EXPECT_DOUBLE_EQ(1.25, tmp.radians());
}

TEST(S1Angle, Trigonometry) {
  // Spot check a few angles to ensure that the correct function is called.
  EXPECT_DOUBLE_EQ(1, cos(S1Angle::Degrees(0)));
  EXPECT_DOUBLE_EQ(1, sin(S1Angle::Degrees(90)));
  EXPECT_DOUBLE_EQ(1, tan(S1Angle::Degrees(45)));
}

TEST(S1Angle, ConstructorsThatMeasureAngles) {
  EXPECT_DOUBLE_EQ(M_PI_2,
                   S1Angle(S2Point(1, 0, 0), S2Point(0, 0, 2)).radians());
  EXPECT_DOUBLE_EQ(0.0, S1Angle(S2Point(1, 0, 0), S2Point(1, 0, 0)).radians());
  EXPECT_NEAR(50.0,
              S1Angle(S2LatLng::FromDegrees(20, 20),
                      S2LatLng::FromDegrees(70, 20)).degrees(),
              1e-13);
}

TEST(S1Angle, TestFormatting) {
  std::ostringstream ss;
  ss << S1Angle::Degrees(180.0);
  EXPECT_EQ("180.0000000", ss.str());
}

// The current implementation guarantees exact conversions between
// Degrees() and E6() when the Degrees() argument is an integer.
TEST(S1Angle, DegreesVsE6) {
  for (int i = 0; i <= 180; ++i) {
    EXPECT_EQ(S1Angle::Degrees(i), S1Angle::E6(1000000 * i));
  }
}

// The current implementation guarantees exact conversions between
// Degrees() and E7() when the Degrees() argument is an integer.
TEST(S1Angle, DegreesVsE7) {
  for (int i = 0; i <= 180; ++i) {
    EXPECT_EQ(S1Angle::Degrees(i), S1Angle::E7(10000000 * i));
  }
}

// The current implementation guarantees exact conversions between
// E6() and E7() when the E6() argument is an integer.
TEST(S1Angle, E6VsE7) {
  S2Testing::rnd.Reset(FLAGS_s2_random_seed);
  for (int iter = 0; iter < 1000; ++iter) {
    int i = S2Testing::rnd.Uniform(180000000);
    EXPECT_EQ(S1Angle::E6(i), S1Angle::E7(10 * i));
  }
}

// The current implementation guarantees certain exact conversions between
// degrees and radians (see the header file for details).
TEST(S1Angle, DegreesVsRadians) {
  for (int k = -8; k <= 8; ++k) {
    EXPECT_EQ(S1Angle::Degrees(45 * k), S1Angle::Radians(k * M_PI / 4));
    EXPECT_EQ(45 * k, S1Angle::Degrees(45 * k).degrees());
  }
  for (int k = 0; k <= 30; ++k) {
    int n = 1 << k;
    EXPECT_EQ(S1Angle::Degrees(180. / n), S1Angle::Radians(M_PI / n));
    EXPECT_EQ(S1Angle::Degrees(60. / n), S1Angle::Radians(M_PI / (3. * n)));
    EXPECT_EQ(S1Angle::Degrees(36. / n), S1Angle::Radians(M_PI / (5. * n)));
    EXPECT_EQ(S1Angle::Degrees(20. / n), S1Angle::Radians(M_PI / (9. * n)));
    EXPECT_EQ(S1Angle::Degrees(4. / n), S1Angle::Radians(M_PI / (45. * n)));
  }
  // We also spot check a couple of non-identities.
  EXPECT_NE(S1Angle::Degrees(3), S1Angle::Radians(M_PI / 60));
  EXPECT_NE(60, S1Angle::Degrees(60).degrees());
}
