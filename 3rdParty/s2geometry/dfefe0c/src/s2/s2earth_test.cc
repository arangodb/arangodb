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

#include "s2/s2earth.h"

#include <cmath>

#include <gtest/gtest.h>
#include "s2/util/units/physical-units.h"

TEST(S2EarthTest, TestAngleConversion) {
  ASSERT_DOUBLE_EQ(S2Earth::ToAngle(S2Earth::Radius()).radians(), 1);
  ASSERT_DOUBLE_EQ(S2Earth::ToChordAngle(S2Earth::Radius()).radians(), 1);
  ASSERT_FLOAT_EQ(
      util::units::Meters(S2Earth::ToDistance(S1Angle::Radians(2))).value(),
      2 * S2Earth::RadiusMeters());
  ASSERT_FLOAT_EQ(
      util::units::Meters(S2Earth::ToDistance(S1ChordAngle::Radians(2)))
          .value(),
      2 * S2Earth::RadiusMeters());
  ASSERT_DOUBLE_EQ(
      S2Earth::ToRadians(util::units::Meters(S2Earth::RadiusMeters())), 1);
  ASSERT_DOUBLE_EQ(S2Earth::ToMeters(S1Angle::Degrees(180)),
                   S2Earth::RadiusMeters() * M_PI);
  ASSERT_DOUBLE_EQ(S2Earth::ToMeters(S1ChordAngle::Degrees(180)),
                   S2Earth::RadiusMeters() * M_PI);
  ASSERT_DOUBLE_EQ(S2Earth::ToKm(S1Angle::Radians(0.5)),
                   0.5 * S2Earth::RadiusKm());
  ASSERT_DOUBLE_EQ(S2Earth::ToKm(S1ChordAngle::Radians(0.5)),
                   0.5 * S2Earth::RadiusKm());
  ASSERT_DOUBLE_EQ(S2Earth::KmToRadians(S2Earth::RadiusMeters() / 1000), 1);
  ASSERT_DOUBLE_EQ(S2Earth::RadiansToKm(0.5), 0.5 * S2Earth::RadiusKm());
  ASSERT_DOUBLE_EQ(S2Earth::MetersToRadians(S2Earth::RadiansToKm(0.3) * 1000),
                   0.3);
  ASSERT_DOUBLE_EQ(S2Earth::RadiansToMeters(S2Earth::KmToRadians(2.5)), 2500);
}

TEST(S2EarthTest, TestSolidAngleConversion) {
  ASSERT_DOUBLE_EQ(1,
                   S2Earth::SquareKmToSteradians(
                       std::pow(S2Earth::RadiusMeters() / 1000, 2)));
  ASSERT_DOUBLE_EQ(std::pow(0.5 * S2Earth::RadiusKm(), 2),
                   S2Earth::SteradiansToSquareKm(std::pow(0.5, 2)));
  ASSERT_DOUBLE_EQ(std::pow(0.3, 2),
                   S2Earth::SquareMetersToSteradians(
                       std::pow(S2Earth::RadiansToKm(0.3) * 1000, 2)));
  ASSERT_DOUBLE_EQ(std::pow(2500, 2),
                   S2Earth::SteradiansToSquareMeters(
                       std::pow(S2Earth::KmToRadians(2.5), 2)));
}

TEST(S2EarthTest, TestToLongitudeRadians) {
  const util::units::Meters earth_radius =
      util::units::Meters(S2Earth::RadiusMeters());

  // At the equator, ToLongitudeRadians behaves exactly like ToRadians.
  ASSERT_DOUBLE_EQ(S2Earth::ToLongitudeRadians(earth_radius, 0), 1);

  // The closer we get to the poles, the more radians we need to go the same
  // distance.
  ASSERT_GT(S2Earth::ToLongitudeRadians(earth_radius, 0.5),
            S2Earth::ToLongitudeRadians(earth_radius, 0.4));

  // At the poles, we should return 2PI radians instead of dividing by 0.
  ASSERT_DOUBLE_EQ(S2Earth::ToLongitudeRadians(earth_radius, M_PI_2), M_PI * 2);

  // Within epsilon of the poles, we should still return 2PI radians instead
  // of directing the caller to take thousands of radians around.
  ASSERT_DOUBLE_EQ(S2Earth::ToLongitudeRadians(earth_radius, M_PI_2 - 1e-4),
                   M_PI * 2);
}

TEST(S2EarthTest, TestGetInitialBearing) {
  struct TestConfig {
    string description;
    S2LatLng a;
    S2LatLng b;
    S1Angle bearing;
  } test_configs[] = {
      {"Westward on equator", S2LatLng::FromDegrees(0, 50),
       S2LatLng::FromDegrees(0, 100), S1Angle::Degrees(90)},
      {"Eastward on equator", S2LatLng::FromDegrees(0, 50),
       S2LatLng::FromDegrees(0, 0), S1Angle::Degrees(-90)},
      {"Northward on meridian", S2LatLng::FromDegrees(16, 28),
       S2LatLng::FromDegrees(81, 28), S1Angle::Degrees(0)},
      {"Southward on meridian", S2LatLng::FromDegrees(24, 64),
       S2LatLng::FromDegrees(-27, 64), S1Angle::Degrees(180)},
      {"Towards north pole", S2LatLng::FromDegrees(12, 76),
       S2LatLng::FromDegrees(90, 50), S1Angle::Degrees(0)},
      {"Towards south pole", S2LatLng::FromDegrees(-35, 105),
       S2LatLng::FromDegrees(-90, -120), S1Angle::Degrees(180)},
      {"Spain to Japan", S2LatLng::FromDegrees(40.4379332, -3.749576),
       S2LatLng::FromDegrees(35.6733227, 139.6403486), S1Angle::Degrees(29.2)},
      {"Japan to Spain", S2LatLng::FromDegrees(35.6733227, 139.6403486),
       S2LatLng::FromDegrees(40.4379332, -3.749576), S1Angle::Degrees(-27.2)},
  };

  for (const TestConfig& config : test_configs) {
    const S1Angle bearing = S2Earth::GetInitialBearing(config.a, config.b);
    const S1Angle angle_diff = (bearing - config.bearing).Normalized().abs();
    EXPECT_LE(angle_diff.degrees(), 1e-2)
        << "GetInitialBearing() test failed on: " << config.description
        << ". Expected " << config.bearing << ", got " << bearing;
  }
}

TEST(S2EarthTest, TestGetDistance) {
  S2Point north(0, 0, 1);
  S2Point south(0, 0, -1);
  S2Point west(0, -1, 0);

  ASSERT_FLOAT_EQ(
      util::units::Miles(S2Earth::GetDistance(north, south)).value(),
      util::units::Miles(M_PI * S2Earth::Radius()).value());
  ASSERT_DOUBLE_EQ(S2Earth::GetDistanceKm(west, west), 0);
  ASSERT_DOUBLE_EQ(S2Earth::GetDistanceMeters(north, west),
                   M_PI_2 * S2Earth::RadiusMeters());

  ASSERT_FLOAT_EQ(
      util::units::Feet(S2Earth::GetDistance(S2LatLng::FromDegrees(0, -90),
                                             S2LatLng::FromDegrees(-90, -38)))
          .value(),
      util::units::Feet(S2Earth::GetDistance(west, south)).value());

  ASSERT_DOUBLE_EQ(S2Earth::GetDistanceKm(S2LatLng::FromRadians(0, 0.6),
                                          S2LatLng::FromRadians(0, -0.4)),
                   S2Earth::RadiusKm());

  ASSERT_DOUBLE_EQ(S2Earth::GetDistanceMeters(S2LatLng::FromDegrees(80, 27),
                                              S2LatLng::FromDegrees(55, -153)),
                   1000 * S2Earth::RadiusKm() * M_PI / 4);
}
