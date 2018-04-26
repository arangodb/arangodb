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

#include <cmath>
#include <cstdio>
#include <ostream>

#include "s2/s2latlng.h"

S1Angle::S1Angle(const S2Point& x, const S2Point& y)
    : radians_(x.Angle(y)) {
}

S1Angle::S1Angle(const S2LatLng& x, const S2LatLng& y)
    : radians_(x.GetDistance(y).radians()) {
}

S1Angle S1Angle::Normalized() const {
  S1Angle a(radians_);
  a.Normalize();
  return a;
}

void S1Angle::Normalize() {
  radians_ = remainder(radians_, 2.0 * M_PI);
  if (radians_ <= -M_PI) radians_ = M_PI;
}

std::ostream& operator<<(std::ostream& os, S1Angle a) {
  double degrees = a.degrees();
  char buffer[13];
  int sz = snprintf(buffer, sizeof(buffer), "%.7f", degrees);
  if (sz >= 0 && sz < sizeof(buffer)) {
    return os << buffer;
  } else {
    return os << degrees;
  }
}
