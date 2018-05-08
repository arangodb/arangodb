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

#include "s2/s2projections.h"

#include <cmath>
#include "s2/s2latlng.h"

namespace S2 {

// Default implementation, suitable for any projection where edges are defined
// as straight lines in the 2D projected space.
R2Point Projection::Interpolate(double f,
                                const R2Point& a, const R2Point& b) const {
  return (1 - f) * a + f * b;
}

PlateCarreeProjection::PlateCarreeProjection(double x_scale)
    : x_wrap_(2 * x_scale),
      to_radians_(M_PI / x_scale),
      from_radians_(x_scale / M_PI) {
}

R2Point PlateCarreeProjection::Project(const S2Point& p) const {
  return FromLatLng(S2LatLng(p));
}

R2Point PlateCarreeProjection::FromLatLng(const S2LatLng& ll) const {
  return R2Point(from_radians_ * ll.lng().radians(),
                 from_radians_ * ll.lat().radians());
}

S2Point PlateCarreeProjection::Unproject(const R2Point& p) const {
  return ToLatLng(p).ToPoint();
}

S2LatLng PlateCarreeProjection::ToLatLng(const R2Point& p) const {
  return S2LatLng::FromRadians(to_radians_ * p.y(),
                               to_radians_ * remainder(p.x(), x_wrap_));
}

R2Point PlateCarreeProjection::wrap_distance() const {
  return R2Point(x_wrap_, 0);
}

MercatorProjection::MercatorProjection(double max_x)
    : x_wrap_(2 * max_x),
      to_radians_(M_PI / max_x),
      from_radians_(max_x / M_PI) {
}

R2Point MercatorProjection::Project(const S2Point& p) const {
  return FromLatLng(S2LatLng(p));
}

R2Point MercatorProjection::FromLatLng(const S2LatLng& ll) const {
  // This formula is more accurate near zero than the log(tan()) version.
  // Note that latitudes of +/- 90 degrees yield "y" values of +/- infinity.
  double sin_phi = sin(ll.lat());
  double y = 0.5 * log((1 + sin_phi) / (1 - sin_phi));
  return R2Point(from_radians_ * ll.lng().radians(), from_radians_ * y);
}

S2Point MercatorProjection::Unproject(const R2Point& p) const {
  return ToLatLng(p).ToPoint();
}

S2LatLng MercatorProjection::ToLatLng(const R2Point& p) const {
  // This formula is more accurate near zero than the atan(exp()) version.
  double x = to_radians_ * remainder(p.x(), x_wrap_);
  double k = exp(2 * to_radians_ * p.y());
  double y = std::isinf(k) ? M_PI_2 : asin((k - 1) / (k + 1));
  return S2LatLng::FromRadians(y, x);
}

R2Point MercatorProjection::wrap_distance() const {
  return R2Point(x_wrap_, 0);
}

}  // namespace S2
