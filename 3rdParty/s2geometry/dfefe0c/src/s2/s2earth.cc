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

#include "s2/s2earth.h"

#include <cmath>
#include <algorithm>

namespace {

// http://en.wikipedia.org/wiki/Haversine_formula
// Haversine(x) has very good numerical stability around zero.
// Haversine(x) == (1-cos(x))/2 == sin(x/2)^2; must be implemented with the
// second form to reap the numerical benefits.
double Haversine(const double radians) {
  const double sinHalf = sin(radians / 2);
  return sinHalf * sinHalf;
}

}  // namespace

double S2Earth::ToLongitudeRadians(const util::units::Meters& distance,
                                   double latitude_radians) {
  double scalar = cos(latitude_radians);
  if (scalar == 0) return M_PI * 2;
  return std::min(ToRadians(distance) / scalar, M_PI * 2);
}

// Sourced from http://www.movable-type.co.uk/scripts/latlong.html.
S1Angle S2Earth::GetInitialBearing(const S2LatLng& a, const S2LatLng& b) {
  const double lat1 = a.lat().radians();
  const double cosLat2 = cos(b.lat().radians());
  const double lat_diff = b.lat().radians() - a.lat().radians();
  const double lng_diff = b.lng().radians() - a.lng().radians();

  const double x =
      sin(lat_diff) + sin(lat1) * cosLat2 * 2 * Haversine(lng_diff);
  const double y = sin(lng_diff) * cosLat2;
  return S1Angle::Radians(atan2(y, x));
}
