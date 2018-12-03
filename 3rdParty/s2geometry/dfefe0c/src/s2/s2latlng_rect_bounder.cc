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

#include "s2/s2latlng_rect_bounder.h"

#include <cfloat>
#include <cmath>

#include "s2/base/logging.h"
#include "s2/r1interval.h"
#include "s2/s1chord_angle.h"
#include "s2/s1interval.h"
#include "s2/s2pointutil.h"

using std::fabs;
using std::max;
using std::min;

void S2LatLngRectBounder::AddPoint(const S2Point& b) {
  S2_DCHECK(S2::IsUnitLength(b));
  AddInternal(b, S2LatLng(b));
}

void S2LatLngRectBounder::AddLatLng(const S2LatLng& b_latlng) {
  AddInternal(b_latlng.ToPoint(), b_latlng);
}

void S2LatLngRectBounder::AddInternal(const S2Point& b,
                                      const S2LatLng& b_latlng) {
  // Simple consistency check to verify that b and b_latlng are alternate
  // representations of the same vertex.
  S2_DCHECK(S2::ApproxEquals(b, b_latlng.ToPoint()));

  if (bound_.is_empty()) {
    bound_.AddPoint(b_latlng);
  } else {
    // First compute the cross product N = A x B robustly.  This is the normal
    // to the great circle through A and B.  We don't use S2::RobustCrossProd()
    // since that method returns an arbitrary vector orthogonal to A if the two
    // vectors are proportional, and we want the zero vector in that case.
    Vector3_d n = (a_ - b).CrossProd(a_ + b);  // N = 2 * (A x B)

    // The relative error in N gets large as its norm gets very small (i.e.,
    // when the two points are nearly identical or antipodal).  We handle this
    // by choosing a maximum allowable error, and if the error is greater than
    // this we fall back to a different technique.  Since it turns out that
    // the other sources of error in converting the normal to a maximum
    // latitude add up to at most 1.16 * DBL_EPSILON (see below), and it is
    // desirable to have the total error be a multiple of DBL_EPSILON, we have
    // chosen to limit the maximum error in the normal to 3.84 * DBL_EPSILON.
    // It is possible to show that the error is less than this when
    //
    //   n.Norm() >= 8 * sqrt(3) / (3.84 - 0.5 - sqrt(3)) * DBL_EPSILON
    //            = 1.91346e-15 (about 8.618 * DBL_EPSILON)
    double n_norm = n.Norm();
    if (n_norm < 1.91346e-15) {
      // A and B are either nearly identical or nearly antipodal (to within
      // 4.309 * DBL_EPSILON, or about 6 nanometers on the earth's surface).
      if (a_.DotProd(b) < 0) {
        // The two points are nearly antipodal.  The easiest solution is to
        // assume that the edge between A and B could go in any direction
        // around the sphere.
        bound_ = S2LatLngRect::Full();
      } else {
        // The two points are nearly identical (to within 4.309 * DBL_EPSILON).
        // In this case we can just use the bounding rectangle of the points,
        // since after the expansion done by GetBound() this rectangle is
        // guaranteed to include the (lat,lng) values of all points along AB.
        bound_ = bound_.Union(S2LatLngRect::FromPointPair(a_latlng_, b_latlng));
      }
    } else {
      // Compute the longitude range spanned by AB.
      S1Interval lng_ab = S1Interval::FromPointPair(a_latlng_.lng().radians(),
                                                    b_latlng.lng().radians());
      if (lng_ab.GetLength() >= M_PI - 2 * DBL_EPSILON) {
        // The points lie on nearly opposite lines of longitude to within the
        // maximum error of the calculation.  (Note that this test relies on
        // the fact that M_PI is slightly less than the true value of Pi, and
        // that representable values near M_PI are 2 * DBL_EPSILON apart.)
        // The easiest solution is to assume that AB could go on either side
        // of the pole.
        lng_ab = S1Interval::Full();
      }

      // Next we compute the latitude range spanned by the edge AB.  We start
      // with the range spanning the two endpoints of the edge:
      R1Interval lat_ab = R1Interval::FromPointPair(a_latlng_.lat().radians(),
                                                    b_latlng.lat().radians());

      // This is the desired range unless the edge AB crosses the plane
      // through N and the Z-axis (which is where the great circle through A
      // and B attains its minimum and maximum latitudes).  To test whether AB
      // crosses this plane, we compute a vector M perpendicular to this
      // plane and then project A and B onto it.
      Vector3_d m = n.CrossProd(S2Point(0, 0, 1));
      double m_a = m.DotProd(a_);
      double m_b = m.DotProd(b);

      // We want to test the signs of "m_a" and "m_b", so we need to bound
      // the error in these calculations.  It is possible to show that the
      // total error is bounded by
      //
      //  (1 + sqrt(3)) * DBL_EPSILON * n_norm + 8 * sqrt(3) * (DBL_EPSILON**2)
      //    = 6.06638e-16 * n_norm + 6.83174e-31

      double m_error = 6.06638e-16 * n_norm + 6.83174e-31;
      if (m_a * m_b < 0 || fabs(m_a) <= m_error || fabs(m_b) <= m_error) {
        // Minimum/maximum latitude *may* occur in the edge interior.
        //
        // The maximum latitude is 90 degrees minus the latitude of N.  We
        // compute this directly using atan2 in order to get maximum accuracy
        // near the poles.
        //
        // Our goal is compute a bound that contains the computed latitudes of
        // all S2Points P that pass the point-in-polygon containment test.
        // There are three sources of error we need to consider:
        //  - the directional error in N (at most 3.84 * DBL_EPSILON)
        //  - converting N to a maximum latitude
        //  - computing the latitude of the test point P
        // The latter two sources of error are at most 0.955 * DBL_EPSILON
        // individually, but it is possible to show by a more complex analysis
        // that together they can add up to at most 1.16 * DBL_EPSILON, for a
        // total error of 5 * DBL_EPSILON.
        //
        // We add 3 * DBL_EPSILON to the bound here, and GetBound() will pad
        // the bound by another 2 * DBL_EPSILON.
        double max_lat = min(
            atan2(sqrt(n[0]*n[0] + n[1]*n[1]), fabs(n[2])) + 3 * DBL_EPSILON,
            M_PI_2);

        // In order to get tight bounds when the two points are close together,
        // we also bound the min/max latitude relative to the latitudes of the
        // endpoints A and B.  First we compute the distance between A and B,
        // and then we compute the maximum change in latitude between any two
        // points along the great circle that are separated by this distance.
        // This gives us a latitude change "budget".  Some of this budget must
        // be spent getting from A to B; the remainder bounds the round-trip
        // distance (in latitude) from A or B to the min or max latitude
        // attained along the edge AB.
        double lat_budget = 2 * asin(0.5 * (a_ - b).Norm() * sin(max_lat));
        double max_delta = 0.5*(lat_budget - lat_ab.GetLength()) + DBL_EPSILON;

        // Test whether AB passes through the point of maximum latitude or
        // minimum latitude.  If the dot product(s) are small enough then the
        // result may be ambiguous.
        if (m_a <= m_error && m_b >= -m_error) {
          lat_ab.set_hi(min(max_lat, lat_ab.hi() + max_delta));
        }
        if (m_b <= m_error && m_a >= -m_error) {
          lat_ab.set_lo(max(-max_lat, lat_ab.lo() - max_delta));
        }
      }
      bound_ = bound_.Union(S2LatLngRect(lat_ab, lng_ab));
    }
  }
  a_ = b;
  a_latlng_ = b_latlng;
}

S2LatLngRect S2LatLngRectBounder::GetBound() const {
  // To save time, we ignore numerical errors in the computed S2LatLngs while
  // accumulating the bounds and then account for them here.
  //
  // S2LatLng(S2Point) has a maximum error of 0.955 * DBL_EPSILON in latitude.
  // In the worst case, we might have rounded "inwards" when computing the
  // bound and "outwards" when computing the latitude of a contained point P,
  // therefore we expand the latitude bounds by 2 * DBL_EPSILON in each
  // direction.  (A more complex analysis shows that 1.5 * DBL_EPSILON is
  // enough, but the expansion amount should be a multiple of DBL_EPSILON in
  // order to avoid rounding errors during the expansion itself.)
  //
  // S2LatLng(S2Point) has a maximum error of DBL_EPSILON in longitude, which
  // is simply the maximum rounding error for results in the range [-Pi, Pi].
  // This is true because the Gnu implementation of atan2() comes from the IBM
  // Accurate Mathematical Library, which implements correct rounding for this
  // instrinsic (i.e., it returns the infinite precision result rounded to the
  // nearest representable value, with ties rounded to even values).  This
  // implies that we don't need to expand the longitude bounds at all, since
  // we only guarantee that the bound contains the *rounded* latitudes of
  // contained points.  The *true* latitudes of contained points may lie up to
  // DBL_EPSILON outside of the returned bound.

  const S2LatLng kExpansion = S2LatLng::FromRadians(2 * DBL_EPSILON, 0);
  return bound_.Expanded(kExpansion).PolarClosure();
}

S2LatLngRect S2LatLngRectBounder::ExpandForSubregions(
    const S2LatLngRect& bound) {
  // Empty bounds don't need expansion.
  if (bound.is_empty()) return bound;

  // First we need to check whether the bound B contains any nearly-antipodal
  // points (to within 4.309 * DBL_EPSILON).  If so then we need to return
  // S2LatLngRect::Full(), since the subregion might have an edge between two
  // such points, and AddPoint() returns Full() for such edges.  Note that
  // this can happen even if B is not Full(); for example, consider a loop
  // that defines a 10km strip straddling the equator extending from
  // longitudes -100 to +100 degrees.
  //
  // It is easy to check whether B contains any antipodal points, but checking
  // for nearly-antipodal points is trickier.  Essentially we consider the
  // original bound B and its reflection through the origin B', and then test
  // whether the minimum distance between B and B' is less than 4.309 *
  // DBL_EPSILON.

  // "lng_gap" is a lower bound on the longitudinal distance between B and its
  // reflection B'.  (2.5 * DBL_EPSILON is the maximum combined error of the
  // endpoint longitude calculations and the GetLength() call.)
  double lng_gap = max(0.0, M_PI - bound.lng().GetLength() - 2.5 * DBL_EPSILON);

  // "min_abs_lat" is the minimum distance from B to the equator (if zero or
  // negative, then B straddles the equator).
  double min_abs_lat = max(bound.lat().lo(), -bound.lat().hi());

  // "lat_gap1" and "lat_gap2" measure the minimum distance from B to the
  // south and north poles respectively.
  double lat_gap1 = M_PI_2 + bound.lat().lo();
  double lat_gap2 = M_PI_2 - bound.lat().hi();

  if (min_abs_lat >= 0) {
    // The bound B does not straddle the equator.  In this case the minimum
    // distance is between one endpoint of the latitude edge in B closest to
    // the equator and the other endpoint of that edge in B'.  The latitude
    // distance between these two points is 2*min_abs_lat, and the longitude
    // distance is lng_gap.  We could compute the distance exactly using the
    // Haversine formula, but then we would need to bound the errors in that
    // calculation.  Since we only need accuracy when the distance is very
    // small (close to 4.309 * DBL_EPSILON), we substitute the Euclidean
    // distance instead.  This gives us a right triangle XYZ with two edges of
    // length x = 2*min_abs_lat and y ~= lng_gap.  The desired distance is the
    // length of the third edge "z", and we have
    //
    //         z  ~=  sqrt(x^2 + y^2)  >=  (x + y) / sqrt(2)
    //
    // Therefore the region may contain nearly antipodal points only if
    //
    //  2*min_abs_lat + lng_gap  <  sqrt(2) * 4.309 * DBL_EPSILON
    //                           ~= 1.354e-15
    //
    // Note that because the given bound B is conservative, "min_abs_lat" and
    // "lng_gap" are both lower bounds on their true values so we do not need
    // to make any adjustments for their errors.
    if (2 * min_abs_lat + lng_gap < 1.354e-15) {
      return S2LatLngRect::Full();
    }
  } else if (lng_gap >= M_PI_2) {
    // B spans at most Pi/2 in longitude.  The minimum distance is always
    // between one corner of B and the diagonally opposite corner of B'.  We
    // use the same distance approximation that we used above; in this case
    // we have an obtuse triangle XYZ with two edges of length x = lat_gap1
    // and y = lat_gap2, and angle Z >= Pi/2 between them.  We then have
    //
    //         z  >=  sqrt(x^2 + y^2)  >=  (x + y) / sqrt(2)
    //
    // Unlike the case above, "lat_gap1" and "lat_gap2" are not lower bounds
    // (because of the extra addition operation, and because M_PI_2 is not
    // exactly equal to Pi/2); they can exceed their true values by up to
    // 0.75 * DBL_EPSILON.  Putting this all together, the region may
    // contain nearly antipodal points only if
    //
    //   lat_gap1 + lat_gap2  <  (sqrt(2) * 4.309 + 1.5) * DBL_EPSILON
    //                        ~= 1.687e-15
    if (lat_gap1 + lat_gap2 < 1.687e-15) {
      return S2LatLngRect::Full();
    }
  } else {
    // Otherwise we know that (1) the bound straddles the equator and (2) its
    // width in longitude is at least Pi/2.  In this case the minimum
    // distance can occur either between a corner of B and the diagonally
    // opposite corner of B' (as in the case above), or between a corner of B
    // and the opposite longitudinal edge reflected in B'.  It is sufficient
    // to only consider the corner-edge case, since this distance is also a
    // lower bound on the corner-corner distance when that case applies.

    // Consider the spherical triangle XYZ where X is a corner of B with
    // minimum absolute latitude, Y is the closest pole to X, and Z is the
    // point closest to X on the opposite longitudinal edge of B'.  This is a
    // right triangle (Z = Pi/2), and from the spherical law of sines we have
    //
    //     sin(z) / sin(Z)  =  sin(y) / sin(Y)
    //     sin(max_lat_gap) / 1  =  sin(d_min) / sin(lng_gap)
    //     sin(d_min)  =  sin(max_lat_gap) * sin(lng_gap)
    //
    // where "max_lat_gap" = max(lat_gap1, lat_gap2) and "d_min" is the
    // desired minimum distance.  Now using the facts that sin(t) >= (2/Pi)*t
    // for 0 <= t <= Pi/2, that we only need an accurate approximation when
    // at least one of "max_lat_gap" or "lng_gap" is extremely small (in
    // which case sin(t) ~= t), and recalling that "max_lat_gap" has an error
    // of up to 0.75 * DBL_EPSILON, we want to test whether
    //
    //   max_lat_gap * lng_gap  <  (4.309 + 0.75) * (Pi/2) * DBL_EPSILON
    //                          ~= 1.765e-15
    if (max(lat_gap1, lat_gap2) * lng_gap < 1.765e-15) {
      return S2LatLngRect::Full();
    }
  }
  // Next we need to check whether the subregion might contain any edges that
  // span (M_PI - 2 * DBL_EPSILON) radians or more in longitude, since AddPoint
  // sets the longitude bound to Full() in that case.  This corresponds to
  // testing whether (lng_gap <= 0) in "lng_expansion" below.

  // Otherwise, the maximum latitude error in AddPoint is 4.8 * DBL_EPSILON.
  // In the worst case, the errors when computing the latitude bound for a
  // subregion could go in the opposite direction as the errors when computing
  // the bound for the original region, so we need to double this value.
  // (More analysis shows that it's okay to round down to a multiple of
  // DBL_EPSILON.)
  //
  // For longitude, we rely on the fact that atan2 is correctly rounded and
  // therefore no additional bounds expansion is necessary.

  double lat_expansion = 9 * DBL_EPSILON;
  double lng_expansion = (lng_gap <= 0) ? M_PI : 0;
  return bound.Expanded(S2LatLng::FromRadians(lat_expansion,
                                              lng_expansion)).PolarClosure();
}

S2LatLng S2LatLngRectBounder::MaxErrorForTests() {
  // The maximum error in the latitude calculation is
  //    3.84 * DBL_EPSILON   for the RobustCrossProd calculation
  //    0.96 * DBL_EPSILON   for the Latitude() calculation
  //    5    * DBL_EPSILON   added by AddPoint/GetBound to compensate for error
  //    ------------------
  //    9.80 * DBL_EPSILON   maximum error in result
  //
  // The maximum error in the longitude calculation is DBL_EPSILON.  GetBound
  // does not do any expansion because this isn't necessary in order to
  // bound the *rounded* longitudes of contained points.
  return S2LatLng::FromRadians(10 * DBL_EPSILON, 1 * DBL_EPSILON);
}
