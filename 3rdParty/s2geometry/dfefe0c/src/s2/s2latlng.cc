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

#include "s2/s2latlng.h"

#include <algorithm>
#include <ostream>

#include "s2/base/logging.h"
#include "s2/base/stringprintf.h"

using std::max;
using std::min;

S2LatLng S2LatLng::Normalized() const {
  // remainder(x, 2 * M_PI) reduces its argument to the range [-M_PI, M_PI]
  // inclusive, which is what we want here.
  return S2LatLng(max(-M_PI_2, min(M_PI_2, lat().radians())),
                  remainder(lng().radians(), 2 * M_PI));
}

S2Point S2LatLng::ToPoint() const {
  S2_DLOG_IF(ERROR, !is_valid())
      << "Invalid S2LatLng in S2LatLng::ToPoint: " << *this;
  double phi = lat().radians();
  double theta = lng().radians();
  double cosphi = cos(phi);
  return S2Point(cos(theta) * cosphi, sin(theta) * cosphi, sin(phi));
}

S2LatLng::S2LatLng(const S2Point& p)
  : coords_(Latitude(p).radians(), Longitude(p).radians()) {
  // The latitude and longitude are already normalized.
  S2_DLOG_IF(ERROR, !is_valid())
      << "Invalid S2LatLng in constructor: " << *this;
}

S1Angle S2LatLng::GetDistance(const S2LatLng& o) const {
  // This implements the Haversine formula, which is numerically stable for
  // small distances but only gets about 8 digits of precision for very large
  // distances (e.g. antipodal points).  Note that 8 digits is still accurate
  // to within about 10cm for a sphere the size of the Earth.
  //
  // This could be fixed with another sin() and cos() below, but at that point
  // you might as well just convert both arguments to S2Points and compute the
  // distance that way (which gives about 15 digits of accuracy for all
  // distances).

  S2_DLOG_IF(ERROR, !is_valid())
      << "Invalid S2LatLng in S2LatLng::GetDistance: " << *this;

  S2_DLOG_IF(ERROR, !o.is_valid())
      << "Invalid S2LatLng in S2LatLng::GetDistance: " << o;

  double lat1 = lat().radians();
  double lat2 = o.lat().radians();
  double lng1 = lng().radians();
  double lng2 = o.lng().radians();
  double dlat = sin(0.5 * (lat2 - lat1));
  double dlng = sin(0.5 * (lng2 - lng1));
  double x = dlat * dlat + dlng * dlng * cos(lat1) * cos(lat2);
  return S1Angle::Radians(2 * asin(sqrt(min(1.0, x))));
}

string S2LatLng::ToStringInDegrees() const {
  S2LatLng pt = Normalized();
  return StringPrintf("%f,%f", pt.lat().degrees(), pt.lng().degrees());
}

void S2LatLng::ToStringInDegrees(string* s) const {
  *s = ToStringInDegrees();
}

std::ostream& operator<<(std::ostream& os, const S2LatLng& ll) {
  return os << "[" << ll.lat() << ", " << ll.lng() << "]";
}
