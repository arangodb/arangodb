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
//
// The earth modeled as a sphere.  There are lots of convenience functions so
// that it doesn't take 2 lines of code just to do a single conversion.

#ifndef S2_S2EARTH_H_
#define S2_S2EARTH_H_

#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2latlng.h"
#include "s2/s2point.h"
#include "s2/util/units/length-units.h"

class S2Earth {
 public:
  // These functions convert between distances on the unit sphere
  // (expressed as angles subtended from the sphere's center) and
  // distances on the Earth's surface.  This is possible only because
  // the Earth is modeled as a sphere; otherwise a given angle would
  // correspond to a range of distances depending on where the
  // corresponding line segment was located.

  // Functions for converting distances to angles:
  static constexpr S1Angle MetersToAngle(double meters);
  static inline S1ChordAngle MetersToChordAngle(double meters);
  static constexpr double MetersToRadians(double meters);

  // Functions for converting angles to distances:
  static constexpr double ToMeters(const S1Angle& angle);
  static inline double ToMeters(const S1ChordAngle& cangle);
  static constexpr double RadiansToMeters(double radians);

  // Like the above, but where distances are expressed in kilometers:
  static constexpr S1Angle KmToAngle(double km);
  static inline S1ChordAngle KmToChordAngle(double km);
  static constexpr double KmToRadians(double km);
  static constexpr double ToKm(const S1Angle& angle);
  static inline double ToKm(const S1ChordAngle& cangle);
  static constexpr double RadiansToKm(double radians);

  // Like the above, but where distances are expressed in util::units::Meters.
  //
  // CAVEAT: These versions are not as accurate because util::units::Meters
  // uses "float" rather than "double" as the underlying representation.
  // (You may lose precision if you use these functions.)
  static constexpr S1Angle ToAngle(const util::units::Meters& distance);
  static inline S1ChordAngle ToChordAngle(const util::units::Meters& distance);
  static constexpr double ToRadians(const util::units::Meters& distance);
  static constexpr util::units::Meters ToDistance(const S1Angle& angle);
  static inline util::units::Meters ToDistance(const S1ChordAngle& cangle);
  static constexpr util::units::Meters RadiansToDistance(double radians);

  // These functions convert between areas on the unit sphere
  // (as returned by the S2 library) and areas on the Earth's surface.
  // Note that the area of a region on the unit sphere is equal to the
  // solid angle it subtends from the sphere's center (measured in steradians).
  static constexpr double SquareKmToSteradians(double km2);
  static constexpr double SquareMetersToSteradians(double m2);
  static constexpr double SteradiansToSquareKm(double steradians);
  static constexpr double SteradiansToSquareMeters(double steradians);

  // Convenience functions for the frequent case where you need to call
  // ToRadians in order to convert an east-west distance on the globe to
  // radians. The output is a function of how close to the poles you are
  // (i.e. at the bulge at the equator, one unit of longitude represents a
  // much farther distance). The function will never return more than 2*PI
  // radians, even if you're trying to go 100 million miles west at the north
  // pole.
  static double MetersToLongitudeRadians(double meters,
                                         double latitude_radians);
  static double KmToLongitudeRadians(double km, double latitude_radians);

  // CAVEAT: This version is not as accurate because util::units::Meters
  // uses "float" rather than "double" as the underlying representation.
  static double ToLongitudeRadians(const util::units::Meters& distance,
                                   double latitude_radians);

  // Computes the initial bearing from a to b. This is the bearing an observer
  // at point a has when facing point b. A bearing of 0 degrees is north, and it
  // increases clockwise (90 degrees is east, etc).
  //
  // If a == b, a == -b, or a is one of the Earths' poles, the return value is
  // undefined.
  static S1Angle GetInitialBearing(const S2LatLng& a, const S2LatLng& b);

  // Returns the distance between two points.
  static inline double GetDistanceMeters(const S2Point& a, const S2Point& b);
  static inline double GetDistanceMeters(const S2LatLng& a, const S2LatLng& b);
  static inline double GetDistanceKm(const S2Point& a, const S2Point& b);
  static inline double GetDistanceKm(const S2LatLng& a, const S2LatLng& b);

  // CAVEAT: These versions are not as accurate because util::units::Meters
  // uses "float" rather than "double" as the underlying representation.
  static inline util::units::Meters GetDistance(const S2Point& a,
                                                const S2Point& b);
  static inline util::units::Meters GetDistance(const S2LatLng& a,
                                                const S2LatLng& b);

  // Returns the Earth's mean radius, which is the radius of the equivalent
  // sphere with the same surface area.  According to NASA, this value is
  // 6371.01 +/- 0.02 km.  The equatorial radius is 6378.136 km, and the polar
  // radius is 6356.752 km.  They differ by one part in 298.257.
  //
  // Reference: http://ssd.jpl.nasa.gov/phys_props_earth.html, which quotes
  // Yoder, C.F. 1995. "Astrometric and Geodetic Properties of Earth and the
  // Solar System" in Global Earth Physics, A Handbook of Physical Constants,
  // AGU Reference Shelf 1, American Geophysical Union, Table 2.
  static constexpr double RadiusMeters();
  static constexpr double RadiusKm();

  // CAVEAT: This version is not as accurate because util::units::Meters
  // uses "float" rather than "double" as the underlying representation.
  static constexpr util::units::Meters Radius();

  // Convenience functions.

  // Returns the altitude of the lowest known point on Earth. The lowest known
  // point on Earth is the Challenger Deep with an altitude of -10898 meters
  // above the surface of the spherical earth.
  static constexpr double LowestAltitudeMeters();
  static constexpr double LowestAltitudeKm();
  static constexpr util::units::Meters LowestAltitude();

  // Returns the altitude of the highest known point on Earth. The highest
  // known point on Earth is Mount Everest with an altitude of 8846 meters
  // above the surface of the spherical earth.
  static constexpr double HighestAltitudeMeters();
  static constexpr double HighestAltitudeKm();
  static constexpr util::units::Meters HighestAltitude();
};

constexpr S1Angle S2Earth::MetersToAngle(double meters) {
  return S1Angle::Radians(MetersToRadians(meters));
}

inline S1ChordAngle S2Earth::MetersToChordAngle(double meters) {
  return S1ChordAngle(MetersToAngle(meters));
}

constexpr double S2Earth::MetersToRadians(double meters) {
  return meters / RadiusMeters();
}

constexpr double S2Earth::ToMeters(const S1Angle& angle) {
  return angle.radians() * RadiusMeters();
}

inline double S2Earth::ToMeters(const S1ChordAngle& cangle) {
  return ToMeters(cangle.ToAngle());
}

constexpr double S2Earth::RadiansToMeters(double radians) {
  return radians * RadiusMeters();
}

constexpr S1Angle S2Earth::KmToAngle(double km) {
  return S1Angle::Radians(KmToRadians(km));
}

inline S1ChordAngle S2Earth::KmToChordAngle(double km) {
  return S1ChordAngle(KmToAngle(km));
}

constexpr double S2Earth::KmToRadians(double km) { return km / RadiusKm(); }

constexpr double S2Earth::ToKm(const S1Angle& angle) {
  return angle.radians() * RadiusKm();
}

inline double S2Earth::ToKm(const S1ChordAngle& cangle) {
  return ToKm(cangle.ToAngle());
}

constexpr double S2Earth::RadiansToKm(double radians) {
  return radians * RadiusKm();
}

constexpr S1Angle S2Earth::ToAngle(const util::units::Meters& distance) {
  return S1Angle::Radians(ToRadians(distance));
}

inline S1ChordAngle S2Earth::ToChordAngle(const util::units::Meters& distance) {
  return S1ChordAngle(ToAngle(distance));
}

constexpr double S2Earth::ToRadians(const util::units::Meters& distance) {
  return distance.value() / RadiusMeters();
}

constexpr util::units::Meters S2Earth::ToDistance(const S1Angle& angle) {
  return util::units::Meters(ToMeters(angle));
}

inline util::units::Meters S2Earth::ToDistance(const S1ChordAngle& cangle) {
  return util::units::Meters(ToMeters(cangle));
}

constexpr util::units::Meters S2Earth::RadiansToDistance(double radians) {
  return util::units::Meters(RadiansToMeters(radians));
}

constexpr double S2Earth::SquareKmToSteradians(double km2) {
  return km2 / (RadiusKm() * RadiusKm());
}

constexpr double S2Earth::SquareMetersToSteradians(double m2) {
  return m2 / (RadiusMeters() * RadiusMeters());
}

constexpr double S2Earth::SteradiansToSquareKm(double steradians) {
  return steradians * RadiusKm() * RadiusKm();
}

constexpr double S2Earth::SteradiansToSquareMeters(double steradians) {
  return steradians * RadiusMeters() * RadiusMeters();
}

inline double S2Earth::KmToLongitudeRadians(double km,
                                            double latitude_radians) {
  return MetersToLongitudeRadians(1000 * km, latitude_radians);
}

inline double S2Earth::ToLongitudeRadians(const util::units::Meters& distance,
                                          double latitude_radians) {
  return MetersToLongitudeRadians(distance.value(), latitude_radians);
}

inline double S2Earth::GetDistanceMeters(const S2Point& a, const S2Point& b) {
  return RadiansToMeters(a.Angle(b));
}

inline double S2Earth::GetDistanceMeters(const S2LatLng& a, const S2LatLng& b) {
  return ToMeters(a.GetDistance(b));
}

inline double S2Earth::GetDistanceKm(const S2Point& a, const S2Point& b) {
  return RadiansToKm(a.Angle(b));
}

inline double S2Earth::GetDistanceKm(const S2LatLng& a, const S2LatLng& b) {
  return ToKm(a.GetDistance(b));
}

inline util::units::Meters S2Earth::GetDistance(const S2Point& a,
                                                const S2Point& b) {
  return RadiansToDistance(a.Angle(b));
}

inline util::units::Meters S2Earth::GetDistance(const S2LatLng& a,
                                                const S2LatLng& b) {
  return ToDistance(a.GetDistance(b));
}

constexpr double S2Earth::RadiusMeters() { return 6371010.0; }

constexpr double S2Earth::RadiusKm() { return 0.001 * RadiusMeters(); }

constexpr util::units::Meters S2Earth::Radius() {
  return util::units::Meters(RadiusMeters());
}

constexpr double S2Earth::LowestAltitudeMeters() { return -10898; }

constexpr double S2Earth::LowestAltitudeKm() {
  return 0.001 * LowestAltitudeMeters();
}

constexpr util::units::Meters S2Earth::LowestAltitude() {
  return util::units::Meters(LowestAltitudeMeters());
}

constexpr double S2Earth::HighestAltitudeMeters() { return 8846; }

constexpr double S2Earth::HighestAltitudeKm() {
  return 0.001 * HighestAltitudeMeters();
}

constexpr util::units::Meters S2Earth::HighestAltitude() {
  return util::units::Meters(HighestAltitudeMeters());
}

#endif  // S2_S2EARTH_H_
