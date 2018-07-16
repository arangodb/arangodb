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

#include "s2/s2cap.h"

#include <cfloat>
#include <gtest/gtest.h>
#include "s2/r1interval.h"
#include "s2/s1interval.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2coords.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2metrics.h"
#include "s2/s2testing.h"
#include "s2/util/math/vector.h"

using std::vector;

static S2Point GetLatLngPoint(double lat_degrees, double lng_degrees) {
  return S2LatLng::FromDegrees(lat_degrees, lng_degrees).ToPoint();
}

// About 9 times the double-precision roundoff relative error.
static const double kEps = 1e-15;

TEST(S2Cap, Basic) {
  // Test basic properties of empty and full caps.
  S2Cap empty = S2Cap::Empty();
  S2Cap full = S2Cap::Full();
  EXPECT_TRUE(empty.is_valid());
  EXPECT_TRUE(empty.is_empty());
  EXPECT_TRUE(empty.Complement().is_full());
  EXPECT_TRUE(full.is_valid());
  EXPECT_TRUE(full.is_full());
  EXPECT_TRUE(full.Complement().is_empty());
  EXPECT_EQ(2, full.height());
  EXPECT_DOUBLE_EQ(180.0, full.GetRadius().degrees());

  // Test the S1Angle constructor using out-of-range arguments.
  EXPECT_TRUE(S2Cap(S2Point(1, 0, 0), S1Angle::Radians(-20)).is_empty());
  EXPECT_TRUE(S2Cap(S2Point(1, 0, 0), S1Angle::Radians(5)).is_full());
  EXPECT_TRUE(S2Cap(S2Point(1, 0, 0), S1Angle::Infinity()).is_full());

  // Check that the default S2Cap is identical to Empty().
  S2Cap default_empty;
  EXPECT_TRUE(default_empty.is_valid());
  EXPECT_TRUE(default_empty.is_empty());
  EXPECT_EQ(empty.center(), default_empty.center());
  EXPECT_EQ(empty.height(), default_empty.height());

  // Containment and intersection of empty and full caps.
  EXPECT_TRUE(empty.Contains(empty));
  EXPECT_TRUE(full.Contains(empty));
  EXPECT_TRUE(full.Contains(full));
  EXPECT_FALSE(empty.InteriorIntersects(empty));
  EXPECT_TRUE(full.InteriorIntersects(full));
  EXPECT_FALSE(full.InteriorIntersects(empty));

  // Singleton cap containing the x-axis.
  S2Cap xaxis = S2Cap::FromPoint(S2Point(1, 0, 0));
  EXPECT_TRUE(xaxis.Contains(S2Point(1, 0, 0)));
  EXPECT_FALSE(xaxis.Contains(S2Point(1, 1e-20, 0)));
  EXPECT_EQ(0, xaxis.GetRadius().radians());

  // Singleton cap containing the y-axis.
  S2Cap yaxis = S2Cap::FromPoint(S2Point(0, 1, 0));
  EXPECT_FALSE(yaxis.Contains(xaxis.center()));
  EXPECT_EQ(0, xaxis.height());

  // Check that the complement of a singleton cap is the full cap.
  S2Cap xcomp = xaxis.Complement();
  EXPECT_TRUE(xcomp.is_valid());
  EXPECT_TRUE(xcomp.is_full());
  EXPECT_TRUE(xcomp.Contains(xaxis.center()));

  // Check that the complement of the complement is *not* the original.
  EXPECT_TRUE(xcomp.Complement().is_valid());
  EXPECT_TRUE(xcomp.Complement().is_empty());
  EXPECT_FALSE(xcomp.Complement().Contains(xaxis.center()));

  // Check that very small caps can be represented accurately.
  // Here "kTinyRad" is small enough that unit vectors perturbed by this
  // amount along a tangent do not need to be renormalized.
  static const double kTinyRad = 1e-10;
  S2Cap tiny(S2Point(1, 2, 3).Normalize(), S1Angle::Radians(kTinyRad));
  Vector3_d tangent = tiny.center().CrossProd(S2Point(3, 2, 1)).Normalize();
  EXPECT_TRUE(tiny.Contains(tiny.center() + 0.99 * kTinyRad * tangent));
  EXPECT_FALSE(tiny.Contains(tiny.center() + 1.01 * kTinyRad * tangent));

  // Basic tests on a hemispherical cap.
  S2Cap hemi = S2Cap::FromCenterHeight(S2Point(1, 0, 1).Normalize(), 1);
  EXPECT_EQ(-hemi.center(), S2Point(hemi.Complement().center()));
  EXPECT_EQ(1, hemi.Complement().height());
  EXPECT_TRUE(hemi.Contains(S2Point(1, 0, 0)));
  EXPECT_FALSE(hemi.Complement().Contains(S2Point(1, 0, 0)));
  EXPECT_TRUE(hemi.Contains(S2Point(1, 0, -(1-kEps)).Normalize()));
  EXPECT_FALSE(hemi.InteriorContains(S2Point(1, 0, -(1+kEps)).Normalize()));

  // A concave cap.  Note that the error bounds for point containment tests
  // increase with the cap angle, so we need to use a larger error bound
  // here.  (It would be painful to do this everywhere, but this at least
  // gives an example of how to compute the maximum error.)
  S2Point center = GetLatLngPoint(80, 10);
  S1ChordAngle radius(S1Angle::Degrees(150));
  double max_error = (radius.GetS2PointConstructorMaxError() +
                      radius.GetS1AngleConstructorMaxError() +
                      3 * DBL_EPSILON);  // GetLatLngPoint() error
  S2Cap concave(center, radius);
  S2Cap concave_min(center, radius.PlusError(-max_error));
  S2Cap concave_max(center, radius.PlusError(max_error));
  EXPECT_TRUE(concave_max.Contains(GetLatLngPoint(-70, 10)));
  EXPECT_FALSE(concave_min.Contains(GetLatLngPoint(-70, 10)));
  EXPECT_TRUE(concave_max.Contains(GetLatLngPoint(-50, -170)));
  EXPECT_FALSE(concave_min.Contains(GetLatLngPoint(-50, -170)));

  // Cap containment tests.
  EXPECT_FALSE(empty.Contains(xaxis));
  EXPECT_FALSE(empty.InteriorIntersects(xaxis));
  EXPECT_TRUE(full.Contains(xaxis));
  EXPECT_TRUE(full.InteriorIntersects(xaxis));
  EXPECT_FALSE(xaxis.Contains(full));
  EXPECT_FALSE(xaxis.InteriorIntersects(full));
  EXPECT_TRUE(xaxis.Contains(xaxis));
  EXPECT_FALSE(xaxis.InteriorIntersects(xaxis));
  EXPECT_TRUE(xaxis.Contains(empty));
  EXPECT_FALSE(xaxis.InteriorIntersects(empty));
  EXPECT_TRUE(hemi.Contains(tiny));
  EXPECT_TRUE(hemi.Contains(
      S2Cap(S2Point(1, 0, 0), S1Angle::Radians(M_PI_4 - kEps))));
  EXPECT_FALSE(hemi.Contains(
      S2Cap(S2Point(1, 0, 0), S1Angle::Radians(M_PI_4 + kEps))));
  EXPECT_TRUE(concave.Contains(hemi));
  EXPECT_TRUE(concave.InteriorIntersects(hemi.Complement()));
  EXPECT_FALSE(concave.Contains(
      S2Cap::FromCenterHeight(-concave.center(), 0.1)));
}

TEST(S2Cap, GetRectBound) {
  // Empty and full caps.
  EXPECT_TRUE(S2Cap::Empty().GetRectBound().is_empty());
  EXPECT_TRUE(S2Cap::Full().GetRectBound().is_full());

  static const double kDegreeEps = 1e-13;
  // Maximum allowable error for latitudes and longitudes measured in
  // degrees.  (EXPECT_DOUBLE_EQ isn't sufficient.)

  // Cap that includes the south pole.
  S2LatLngRect rect = S2Cap(GetLatLngPoint(-45, 57),
                            S1Angle::Degrees(50)).GetRectBound();
  EXPECT_NEAR(rect.lat_lo().degrees(), -90, kDegreeEps);
  EXPECT_NEAR(rect.lat_hi().degrees(), 5, kDegreeEps);
  EXPECT_TRUE(rect.lng().is_full());

  // Cap that is tangent to the north pole.
  rect = S2Cap(S2Point(1, 0, 1).Normalize(),
               S1Angle::Radians(M_PI_4 + 1e-16)).GetRectBound();
  EXPECT_NEAR(rect.lat().lo(), 0, kEps);
  EXPECT_NEAR(rect.lat().hi(), M_PI_2, kEps);
  EXPECT_TRUE(rect.lng().is_full());

  rect = S2Cap(S2Point(1, 0, 1).Normalize(),
               S1Angle::Degrees(45 + 5e-15)).GetRectBound();
  EXPECT_NEAR(rect.lat_lo().degrees(), 0, kDegreeEps);
  EXPECT_NEAR(rect.lat_hi().degrees(), 90, kDegreeEps);
  EXPECT_TRUE(rect.lng().is_full());

  // The eastern hemisphere.
  rect = S2Cap(S2Point(0, 1, 0),
               S1Angle::Radians(M_PI_2 + 2e-16)).GetRectBound();
  EXPECT_NEAR(rect.lat_lo().degrees(), -90, kDegreeEps);
  EXPECT_NEAR(rect.lat_hi().degrees(), 90, kDegreeEps);
  EXPECT_TRUE(rect.lng().is_full());

  // A cap centered on the equator.
  rect = S2Cap(GetLatLngPoint(0, 50), S1Angle::Degrees(20)).GetRectBound();
  EXPECT_NEAR(rect.lat_lo().degrees(), -20, kDegreeEps);
  EXPECT_NEAR(rect.lat_hi().degrees(), 20, kDegreeEps);
  EXPECT_NEAR(rect.lng_lo().degrees(), 30, kDegreeEps);
  EXPECT_NEAR(rect.lng_hi().degrees(), 70, kDegreeEps);

  // A cap centered on the north pole.
  rect = S2Cap(GetLatLngPoint(90, 123), S1Angle::Degrees(10)).GetRectBound();
  EXPECT_NEAR(rect.lat_lo().degrees(), 80, kDegreeEps);
  EXPECT_NEAR(rect.lat_hi().degrees(), 90, kDegreeEps);
  EXPECT_TRUE(rect.lng().is_full());
}

TEST(S2Cap, S2CellMethods) {
  // For each cube face, we construct some cells on
  // that face and some caps whose positions are relative to that face,
  // and then check for the expected intersection/containment results.

  // The distance from the center of a face to one of its vertices.
  static const double kFaceRadius = atan(sqrt(2));

  for (int face = 0; face < 6; ++face) {
    // The cell consisting of the entire face.
    S2Cell root_cell = S2Cell::FromFace(face);

    // A leaf cell at the midpoint of the v=1 edge.
    S2Cell edge_cell(S2::FaceUVtoXYZ(face, 0, 1 - kEps));

    // A leaf cell at the u=1, v=1 corner.
    S2Cell corner_cell(S2::FaceUVtoXYZ(face, 1 - kEps, 1 - kEps));

    // Quick check for full and empty caps.
    EXPECT_TRUE(S2Cap::Full().Contains(root_cell));
    EXPECT_FALSE(S2Cap::Empty().MayIntersect(root_cell));

    // Check intersections with the bounding caps of the leaf cells that are
    // adjacent to 'corner_cell' along the Hilbert curve.  Because this corner
    // is at (u=1,v=1), the curve stays locally within the same cube face.
    S2CellId first = corner_cell.id().advance(-3);
    S2CellId last = corner_cell.id().advance(4);
    for (S2CellId id = first; id < last; id = id.next()) {
      S2Cell cell(id);
      EXPECT_EQ(id == corner_cell.id(),
                cell.GetCapBound().Contains(corner_cell));
      EXPECT_EQ(id.parent().contains(corner_cell.id()),
                cell.GetCapBound().MayIntersect(corner_cell));
    }

    int anti_face = (face + 3) % 6;  // Opposite face.
    for (int cap_face = 0; cap_face < 6; ++cap_face) {
      // A cap that barely contains all of 'cap_face'.
      S2Point center = S2::GetNorm(cap_face);
      S2Cap covering(center, S1Angle::Radians(kFaceRadius + kEps));
      EXPECT_EQ(cap_face == face, covering.Contains(root_cell));
      EXPECT_EQ(cap_face != anti_face, covering.MayIntersect(root_cell));
      EXPECT_EQ(center.DotProd(edge_cell.GetCenter()) > 0.1,
                covering.Contains(edge_cell));
      EXPECT_EQ(covering.MayIntersect(edge_cell), covering.Contains(edge_cell));
      EXPECT_EQ(cap_face == face, covering.Contains(corner_cell));
      EXPECT_EQ(center.DotProd(corner_cell.GetCenter()) > 0,
                covering.MayIntersect(corner_cell));

      // A cap that barely intersects the edges of 'cap_face'.
      S2Cap bulging(center, S1Angle::Radians(M_PI_4 + kEps));
      EXPECT_FALSE(bulging.Contains(root_cell));
      EXPECT_EQ(cap_face != anti_face, bulging.MayIntersect(root_cell));
      EXPECT_EQ(cap_face == face, bulging.Contains(edge_cell));
      EXPECT_EQ(center.DotProd(edge_cell.GetCenter()) > 0.1,
                bulging.MayIntersect(edge_cell));
      EXPECT_FALSE(bulging.Contains(corner_cell));
      EXPECT_FALSE(bulging.MayIntersect(corner_cell));

      // A singleton cap.
      S2Cap singleton(center, S1Angle::Zero());
      EXPECT_EQ(cap_face == face, singleton.MayIntersect(root_cell));
      EXPECT_FALSE(singleton.MayIntersect(edge_cell));
      EXPECT_FALSE(singleton.MayIntersect(corner_cell));
    }
  }
}

TEST(S2Cap, GetCellUnionBoundLevel1Radius) {
  // Check that a cap whose radius is approximately the width of a level 1
  // S2Cell can be covered by only 3 faces.
  S2Cap cap(S2Point(1, 1, 1).Normalize(),
            S1Angle::Radians(S2::kMinWidth.GetValue(1)));
  vector<S2CellId> covering;
  cap.GetCellUnionBound(&covering);
  EXPECT_EQ(3, covering.size());
}

TEST(S2Cap, Expanded) {
  EXPECT_TRUE(S2Cap::Empty().Expanded(S1Angle::Radians(2)).is_empty());
  EXPECT_TRUE(S2Cap::Full().Expanded(S1Angle::Radians(2)).is_full());
  S2Cap cap50(S2Point(1, 0, 0), S1Angle::Degrees(50));
  S2Cap cap51(S2Point(1, 0, 0), S1Angle::Degrees(51));
  EXPECT_TRUE(cap50.Expanded(S1Angle::Radians(0)).ApproxEquals(cap50));
  EXPECT_TRUE(cap50.Expanded(S1Angle::Degrees(1)).ApproxEquals(cap51));
  EXPECT_FALSE(cap50.Expanded(S1Angle::Degrees(129.99)).is_full());
  EXPECT_TRUE(cap50.Expanded(S1Angle::Degrees(130.01)).is_full());
}

TEST(S2Cap, GetCentroid) {
  // Empty and full caps.
  EXPECT_EQ(S2Point(), S2Cap::Empty().GetCentroid());
  EXPECT_LE(S2Cap::Full().GetCentroid().Norm(), 1e-15);

  // Random caps.
  for (int i = 0; i < 100; ++i) {
    S2Point center = S2Testing::RandomPoint();
    double height = S2Testing::rnd.UniformDouble(0.0, 2.0);
    S2Cap cap = S2Cap::FromCenterHeight(center, height);
    S2Point centroid = cap.GetCentroid();
    S2Point expected = center * (1.0 - height / 2.0) * cap.GetArea();
    EXPECT_LE((expected - centroid).Norm(), 1e-15);
  }
}

TEST(S2Cap, Union) {
  // Two caps which have the same center but one has a larger radius.
  S2Cap a = S2Cap(GetLatLngPoint(50.0, 10.0), S1Angle::Degrees(0.2));
  S2Cap b = S2Cap(GetLatLngPoint(50.0, 10.0), S1Angle::Degrees(0.3));
  EXPECT_TRUE(b.Contains(a));
  EXPECT_EQ(b, a.Union(b));

  // Two caps where one is the full cap.
  EXPECT_TRUE(a.Union(S2Cap::Full()).is_full());

  // Two caps where one is the empty cap.
  EXPECT_EQ(a, a.Union(S2Cap::Empty()));

  // Two caps which have different centers, one entirely encompasses the other.
  S2Cap c = S2Cap(GetLatLngPoint(51.0, 11.0), S1Angle::Degrees(1.5));
  EXPECT_TRUE(c.Contains(a));
  EXPECT_EQ(a.Union(c).center(), c.center());
  EXPECT_EQ(a.Union(c).GetRadius(), c.GetRadius());

  // Two entirely disjoint caps.
  S2Cap d = S2Cap(GetLatLngPoint(51.0, 11.0), S1Angle::Degrees(0.1));
  EXPECT_FALSE(d.Contains(a));
  EXPECT_FALSE(d.Intersects(a));
  EXPECT_TRUE(a.Union(d).ApproxEquals(d.Union(a)));
  EXPECT_NEAR(50.4588, S2LatLng(a.Union(d).center()).lat().degrees(), 0.001);
  EXPECT_NEAR(10.4525, S2LatLng(a.Union(d).center()).lng().degrees(), 0.001);
  EXPECT_NEAR(0.7425, a.Union(d).GetRadius().degrees(), 0.001);

  // Two partially overlapping caps.
  S2Cap e = S2Cap(GetLatLngPoint(50.3, 10.3), S1Angle::Degrees(0.2));
  EXPECT_FALSE(e.Contains(a));
  EXPECT_TRUE(e.Intersects(a));
  EXPECT_TRUE(a.Union(e).ApproxEquals(e.Union(a)));
  EXPECT_NEAR(50.1500, S2LatLng(a.Union(e).center()).lat().degrees(), 0.001);
  EXPECT_NEAR(10.1495, S2LatLng(a.Union(e).center()).lng().degrees(), 0.001);
  EXPECT_NEAR(0.3781, a.Union(e).GetRadius().degrees(), 0.001);

  // Two very large caps, whose radius sums to in excess of 180 degrees, and
  // whose centers are not antipodal.
  S2Cap f = S2Cap(S2Point(0, 0, 1).Normalize(), S1Angle::Degrees(150));
  S2Cap g = S2Cap(S2Point(0, 1, 0).Normalize(), S1Angle::Degrees(150));
  EXPECT_TRUE(f.Union(g).is_full());

  // Two non-overlapping hemisphere caps with antipodal centers.
  S2Cap hemi = S2Cap::FromCenterHeight(S2Point(0, 0, 1).Normalize(), 1);
  EXPECT_TRUE(hemi.Union(hemi.Complement()).is_full());
}

TEST(S2Cap, EncodeDecode) {
  S2Cap cap = S2Cap::FromCenterHeight(S2Point(3, 2, 1).Normalize(), 1);
  Encoder encoder;
  cap.Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  S2Cap decoded_cap;
  EXPECT_TRUE(decoded_cap.Decode(&decoder));
  EXPECT_EQ(cap, decoded_cap);
}
