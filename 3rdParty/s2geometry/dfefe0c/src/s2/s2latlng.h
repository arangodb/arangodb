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

#ifndef S2_S2LATLNG_H_
#define S2_S2LATLNG_H_

#include <cmath>
#include <iosfwd>
#include <ostream>
#include <string>

#include "s2/base/integral_types.h"
#include "s2/_fp_contract_off.h"
#include "s2/r2.h"
#include "s2/s1angle.h"
#include "s2/util/math/vector.h"

// This class represents a point on the unit sphere as a pair
// of latitude-longitude coordinates.  Like the rest of the "geometry"
// package, the intent is to represent spherical geometry as a mathematical
// abstraction, so functions that are specifically related to the Earth's
// geometry (e.g. easting/northing conversions) should be put elsewhere.
//
// This class is intended to be copied by value as desired.  It uses
// the default copy constructor and assignment operator.
class S2LatLng {
 public:
  // Constructor.  The latitude and longitude are allowed to be outside
  // the is_valid() range.  However, note that most methods that accept
  // S2LatLngs expect them to be normalized (see Normalized() below).
  S2LatLng(S1Angle lat, S1Angle lng);

  // The default constructor sets the latitude and longitude to zero.  This is
  // mainly useful when declaring arrays, STL containers, etc.
  S2LatLng();

  // Convert a direction vector (not necessarily unit length) to an S2LatLng.
  explicit S2LatLng(const S2Point& p);

  // Returns an S2LatLng for which is_valid() will return false.
  static S2LatLng Invalid();

  // Convenience functions -- shorter than calling S1Angle::Radians(), etc.
  static S2LatLng FromRadians(double lat_radians, double lng_radians);
  static S2LatLng FromDegrees(double lat_degrees, double lng_degrees);
  static S2LatLng FromE5(int32 lat_e5, int32 lng_e5);
  static S2LatLng FromE6(int32 lat_e6, int32 lng_e6);
  static S2LatLng FromE7(int32 lat_e7, int32 lng_e7);

  // Convenience functions -- to use when args have been fixed32s in protos.
  //
  // The arguments are static_cast into int32, so very large unsigned values
  // are treated as negative numbers.
  static S2LatLng FromUnsignedE6(uint32 lat_e6, uint32 lng_e6);
  static S2LatLng FromUnsignedE7(uint32 lat_e7, uint32 lng_e7);

  // Methods to compute the latitude and longitude of a point separately.
  static S1Angle Latitude(const S2Point& p);
  static S1Angle Longitude(const S2Point& p);

  // Accessor methods.
  S1Angle lat() const { return S1Angle::Radians(coords_[0]); }
  S1Angle lng() const { return S1Angle::Radians(coords_[1]); }
  const R2Point& coords() const { return coords_; }

  // Return true if the latitude is between -90 and 90 degrees inclusive
  // and the longitude is between -180 and 180 degrees inclusive.
  bool is_valid() const;

  // Clamps the latitude to the range [-90, 90] degrees, and adds or subtracts
  // a multiple of 360 degrees to the longitude if necessary to reduce it to
  // the range [-180, 180].
  S2LatLng Normalized() const;

  // Converts a normalized S2LatLng to the equivalent unit-length vector.
  // The maximum error in the result is 1.5 * DBL_EPSILON.  (This does not
  // include the error of converting degrees, E5, E6, or E7 to radians.)
  //
  // Can be used just like an S2Point constructor.  For example:
  //   S2Cap cap;
  //   cap.AddPoint(S2Point(latlng));
  explicit operator S2Point() const;

  // Converts to an S2Point (equivalent to the operator above).
  S2Point ToPoint() const;

  // Returns the distance (measured along the surface of the sphere) to the
  // given S2LatLng, implemented using the Haversine formula.  This is
  // equivalent to
  //
  //   S1Angle(ToPoint(), o.ToPoint())
  //
  // except that this function is slightly faster, and is also somewhat less
  // accurate for distances approaching 180 degrees (see s1angle.h for
  // details).  Both S2LatLngs must be normalized.
  S1Angle GetDistance(const S2LatLng& o) const;

  // Simple arithmetic operations for manipulating latitude-longitude pairs.
  // The results are not normalized (see Normalized()).
  friend S2LatLng operator+(const S2LatLng& a, const S2LatLng& b);
  friend S2LatLng operator-(const S2LatLng& a, const S2LatLng& b);
  friend S2LatLng operator*(double m, const S2LatLng& a);
  friend S2LatLng operator*(const S2LatLng& a, double m);

  bool operator==(const S2LatLng& o) const { return coords_ == o.coords_; }
  bool operator!=(const S2LatLng& o) const { return coords_ != o.coords_; }
  bool operator<(const S2LatLng& o) const { return coords_ < o.coords_; }
  bool operator>(const S2LatLng& o) const { return coords_ > o.coords_; }
  bool operator<=(const S2LatLng& o) const { return coords_ <= o.coords_; }
  bool operator>=(const S2LatLng& o) const { return coords_ >= o.coords_; }

  bool ApproxEquals(const S2LatLng& o,
                    S1Angle max_error = S1Angle::Radians(1e-15)) const {
    return coords_.aequal(o.coords_, max_error.radians());
  }

  // Exports the latitude and longitude in degrees, separated by a comma.
  // e.g. "94.518000,150.300000"
  string ToStringInDegrees() const;
  void ToStringInDegrees(string* s) const;

 private:
  // Internal constructor.
  explicit S2LatLng(const R2Point& coords) : coords_(coords) {}

  // This is internal to avoid ambiguity about which units are expected.
  S2LatLng(double lat_radians, double lng_radians)
    : coords_(lat_radians, lng_radians) {}

  R2Point coords_;
};

// Hasher for S2LatLng.
// Example use: std::unordered_map<S2LatLng, int, S2LatLngHash>.
struct S2LatLngHash {
  size_t operator()(const S2LatLng& lat_lng) const {
    return GoodFastHash<R2Point>()(lat_lng.coords());
  }
};

//////////////////   Implementation details follow   ////////////////////


inline S2LatLng::S2LatLng(S1Angle lat, S1Angle lng)
    : coords_(lat.radians(), lng.radians()) {}

inline S2LatLng::S2LatLng() : coords_(0, 0) {}

inline S2LatLng S2LatLng::FromRadians(double lat_radians, double lng_radians) {
  return S2LatLng(lat_radians, lng_radians);
}

inline S2LatLng S2LatLng::FromDegrees(double lat_degrees, double lng_degrees) {
  return S2LatLng(S1Angle::Degrees(lat_degrees), S1Angle::Degrees(lng_degrees));
}

inline S2LatLng S2LatLng::FromE5(int32 lat_e5, int32 lng_e5) {
  return S2LatLng(S1Angle::E5(lat_e5), S1Angle::E5(lng_e5));
}

inline S2LatLng S2LatLng::FromE6(int32 lat_e6, int32 lng_e6) {
  return S2LatLng(S1Angle::E6(lat_e6), S1Angle::E6(lng_e6));
}

inline S2LatLng S2LatLng::FromE7(int32 lat_e7, int32 lng_e7) {
  return S2LatLng(S1Angle::E7(lat_e7), S1Angle::E7(lng_e7));
}

inline S2LatLng S2LatLng::FromUnsignedE6(uint32 lat_e6, uint32 lng_e6) {
  return S2LatLng(S1Angle::UnsignedE6(lat_e6), S1Angle::UnsignedE6(lng_e6));
}

inline S2LatLng S2LatLng::FromUnsignedE7(uint32 lat_e7, uint32 lng_e7) {
  return S2LatLng(S1Angle::UnsignedE7(lat_e7), S1Angle::UnsignedE7(lng_e7));
}

inline S2LatLng S2LatLng::Invalid() {
  // These coordinates are outside the bounds allowed by is_valid().
  return S2LatLng(M_PI, 2 * M_PI);
}

inline S1Angle S2LatLng::Latitude(const S2Point& p) {
  // We use atan2 rather than asin because the input vector is not necessarily
  // unit length, and atan2 is much more accurate than asin near the poles.
  return S1Angle::Radians(atan2(p[2], sqrt(p[0]*p[0] + p[1]*p[1])));
}

inline S1Angle S2LatLng::Longitude(const S2Point& p) {
  // Note that atan2(0, 0) is defined to be zero.
  return S1Angle::Radians(atan2(p[1], p[0]));
}

inline bool S2LatLng::is_valid() const {
  return (std::fabs(lat().radians()) <= M_PI_2 &&
          std::fabs(lng().radians()) <= M_PI);
}

inline S2LatLng::operator S2Point() const {
  return ToPoint();
}

inline S2LatLng operator+(const S2LatLng& a, const S2LatLng& b) {
  return S2LatLng(a.coords_ + b.coords_);
}

inline S2LatLng operator-(const S2LatLng& a, const S2LatLng& b) {
  return S2LatLng(a.coords_ - b.coords_);
}

inline S2LatLng operator*(double m, const S2LatLng& a) {
  return S2LatLng(m * a.coords_);
}

inline S2LatLng operator*(const S2LatLng& a, double m) {
  return S2LatLng(m * a.coords_);
}

std::ostream& operator<<(std::ostream& os, const S2LatLng& ll);

#endif  // S2_S2LATLNG_H_
