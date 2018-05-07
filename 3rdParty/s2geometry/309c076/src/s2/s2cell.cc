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

#include "s2/s2cell.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iomanip>

#include "s2/base/logging.h"
#include "s2/r1interval.h"
#include "s2/r2.h"
#include "s2/s1chord_angle.h"
#include "s2/s1interval.h"
#include "s2/s2cap.h"
#include "s2/s2coords.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_distances.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2measures.h"
#include "s2/s2metrics.h"

using S2::internal::kPosToIJ;
using S2::internal::kPosToOrientation;
using std::min;
using std::max;

// Since S2Cells are copied by value, the following assertion is a reminder
// not to add fields unnecessarily.  An S2Cell currently consists of 43 data
// bytes, one vtable pointer, plus alignment overhead.  This works out to 48
// bytes on 32 bit architectures and 56 bytes on 64 bit architectures.
//
// The expression below rounds up (43 + sizeof(void*)) to the nearest
// multiple of sizeof(void*).
static_assert(sizeof(S2Cell) <= ((43+2*sizeof(void*)-1) & -sizeof(void*)),
              "S2Cell is getting bloated");

S2Cell::S2Cell(S2CellId id) {
  id_ = id;
  int ij[2], orientation;
  face_ = id.ToFaceIJOrientation(&ij[0], &ij[1], &orientation);
  orientation_ = orientation;  // Compress int to a byte.
  level_ = id.level();
  uv_ = S2CellId::IJLevelToBoundUV(ij, level_);
}

S2Point S2Cell::GetVertexRaw(int k) const {
  return S2::FaceUVtoXYZ(face_, uv_.GetVertex(k));
}

S2Point S2Cell::GetEdgeRaw(int k) const {
  switch (k & 3) {
    case 0:  return S2::GetVNorm(face_, uv_[1][0]);   // Bottom
    case 1:  return S2::GetUNorm(face_, uv_[0][1]);   // Right
    case 2:  return -S2::GetVNorm(face_, uv_[1][1]);  // Top
    default: return -S2::GetUNorm(face_, uv_[0][0]);  // Left
  }
}

bool S2Cell::Subdivide(S2Cell children[4]) const {
  // This function is equivalent to just iterating over the child cell ids
  // and calling the S2Cell constructor, but it is about 2.5 times faster.

  if (id_.is_leaf()) return false;

  // Compute the cell midpoint in uv-space.
  R2Point uv_mid = id_.GetCenterUV();

  // Create four children with the appropriate bounds.
  S2CellId id = id_.child_begin();
  for (int pos = 0; pos < 4; ++pos, id = id.next()) {
    S2Cell *child = &children[pos];
    child->face_ = face_;
    child->level_ = level_ + 1;
    child->orientation_ = orientation_ ^ kPosToOrientation[pos];
    child->id_ = id;
    // We want to split the cell in half in "u" and "v".  To decide which
    // side to set equal to the midpoint value, we look at cell's (i,j)
    // position within its parent.  The index for "i" is in bit 1 of ij.
    int ij = kPosToIJ[orientation_][pos];
    int i = ij >> 1;
    int j = ij & 1;
    child->uv_[0][i] = uv_[0][i];
    child->uv_[0][1-i] = uv_mid[0];
    child->uv_[1][j] = uv_[1][j];
    child->uv_[1][1-j] = uv_mid[1];
  }
  return true;
}

S2Point S2Cell::GetCenterRaw() const {
  return id_.ToPointRaw();
}

double S2Cell::AverageArea(int level) {
  return S2::kAvgArea.GetValue(level);
}

double S2Cell::ApproxArea() const {
  // All cells at the first two levels have the same area.
  if (level_ < 2) return AverageArea(level_);

  // First, compute the approximate area of the cell when projected
  // perpendicular to its normal.  The cross product of its diagonals gives
  // the normal, and the length of the normal is twice the projected area.
  double flat_area = 0.5 * (GetVertex(2) - GetVertex(0)).
                     CrossProd(GetVertex(3) - GetVertex(1)).Norm();

  // Now, compensate for the curvature of the cell surface by pretending
  // that the cell is shaped like a spherical cap.  The ratio of the
  // area of a spherical cap to the area of its projected disc turns out
  // to be 2 / (1 + sqrt(1 - r*r)) where "r" is the radius of the disc.
  // For example, when r=0 the ratio is 1, and when r=1 the ratio is 2.
  // Here we set Pi*r*r == flat_area to find the equivalent disc.
  return flat_area * 2 / (1 + sqrt(1 - min(M_1_PI * flat_area, 1.0)));
}

double S2Cell::ExactArea() const {
  // There is a straightforward mathematical formula for the exact surface
  // area (based on 4 calls to asin), but as the cell size gets small this
  // formula has too much cancellation error.  So instead we compute the area
  // as the sum of two triangles (which is very accurate at all cell levels).
  S2Point v0 = GetVertex(0);
  S2Point v1 = GetVertex(1);
  S2Point v2 = GetVertex(2);
  S2Point v3 = GetVertex(3);
  return S2::Area(v0, v1, v2) + S2::Area(v0, v2, v3);
}

S2Cell* S2Cell::Clone() const {
  return new S2Cell(*this);
}

S2Cap S2Cell::GetCapBound() const {
  // Use the cell center in (u,v)-space as the cap axis.  This vector is
  // very close to GetCenter() and faster to compute.  Neither one of these
  // vectors yields the bounding cap with minimal surface area, but they
  // are both pretty close.
  //
  // It's possible to show that the two vertices that are furthest from
  // the (u,v)-origin never determine the maximum cap size (this is a
  // possible future optimization).

  S2Point center = S2::FaceUVtoXYZ(face_, uv_.GetCenter()).Normalize();
  S2Cap cap = S2Cap::FromPoint(center);
  for (int k = 0; k < 4; ++k) {
    cap.AddPoint(GetVertex(k));
  }
  return cap;
}

inline double S2Cell::GetLatitude(int i, int j) const {
  S2Point p = S2::FaceUVtoXYZ(face_, uv_[0][i], uv_[1][j]);
  return S2LatLng::Latitude(p).radians();
}

inline double S2Cell::GetLongitude(int i, int j) const {
  S2Point p = S2::FaceUVtoXYZ(face_, uv_[0][i], uv_[1][j]);
  return S2LatLng::Longitude(p).radians();
}

S2LatLngRect S2Cell::GetRectBound() const {
  if (level_ > 0) {
    // Except for cells at level 0, the latitude and longitude extremes are
    // attained at the vertices.  Furthermore, the latitude range is
    // determined by one pair of diagonally opposite vertices and the
    // longitude range is determined by the other pair.
    //
    // We first determine which corner (i,j) of the cell has the largest
    // absolute latitude.  To maximize latitude, we want to find the point in
    // the cell that has the largest absolute z-coordinate and the smallest
    // absolute x- and y-coordinates.  To do this we look at each coordinate
    // (u and v), and determine whether we want to minimize or maximize that
    // coordinate based on the axis direction and the cell's (u,v) quadrant.
    double u = uv_[0][0] + uv_[0][1];
    double v = uv_[1][0] + uv_[1][1];
    int i = S2::GetUAxis(face_)[2] == 0 ? (u < 0) : (u > 0);
    int j = S2::GetVAxis(face_)[2] == 0 ? (v < 0) : (v > 0);
    R1Interval lat = R1Interval::FromPointPair(GetLatitude(i, j),
                                               GetLatitude(1-i, 1-j));
    S1Interval lng = S1Interval::FromPointPair(GetLongitude(i, 1-j),
                                               GetLongitude(1-i, j));

    // We grow the bounds slightly to make sure that the bounding rectangle
    // contains S2LatLng(P) for any point P inside the loop L defined by the
    // four *normalized* vertices.  Note that normalization of a vector can
    // change its direction by up to 0.5 * DBL_EPSILON radians, and it is not
    // enough just to add Normalize() calls to the code above because the
    // latitude/longitude ranges are not necessarily determined by diagonally
    // opposite vertex pairs after normalization.
    //
    // We would like to bound the amount by which the latitude/longitude of a
    // contained point P can exceed the bounds computed above.  In the case of
    // longitude, the normalization error can change the direction of rounding
    // leading to a maximum difference in longitude of 2 * DBL_EPSILON.  In
    // the case of latitude, the normalization error can shift the latitude by
    // up to 0.5 * DBL_EPSILON and the other sources of error can cause the
    // two latitudes to differ by up to another 1.5 * DBL_EPSILON, which also
    // leads to a maximum difference of 2 * DBL_EPSILON.
    return S2LatLngRect(lat, lng).
        Expanded(S2LatLng::FromRadians(2 * DBL_EPSILON, 2 * DBL_EPSILON)).
        PolarClosure();
  }

  // The 4 cells around the equator extend to +/-45 degrees latitude at the
  // midpoints of their top and bottom edges.  The two cells covering the
  // poles extend down to +/-35.26 degrees at their vertices.  The maximum
  // error in this calculation is 0.5 * DBL_EPSILON.
  static const double kPoleMinLat = asin(sqrt(1./3)) - 0.5 * DBL_EPSILON;

  // The face centers are the +X, +Y, +Z, -X, -Y, -Z axes in that order.
  S2_DCHECK_EQ(((face_ < 3) ? 1 : -1), S2::GetNorm(face_)[face_ % 3]);

  S2LatLngRect bound;
  switch (face_) {
    case 0:
      bound = S2LatLngRect(R1Interval(-M_PI_4, M_PI_4),
                           S1Interval(-M_PI_4, M_PI_4));
      break;
    case 1:
      bound = S2LatLngRect(R1Interval(-M_PI_4, M_PI_4),
                           S1Interval(M_PI_4, 3*M_PI_4));
      break;
    case 2:
      bound = S2LatLngRect(R1Interval(kPoleMinLat, M_PI_2),
                           S1Interval::Full());
      break;
    case 3:
      bound = S2LatLngRect(R1Interval(-M_PI_4, M_PI_4),
                           S1Interval(3*M_PI_4, -3*M_PI_4));
      break;
    case 4:
      bound = S2LatLngRect(R1Interval(-M_PI_4, M_PI_4),
                           S1Interval(-3*M_PI_4, -M_PI_4));
      break;
    default:
      bound = S2LatLngRect(R1Interval(-M_PI_2, -kPoleMinLat),
                           S1Interval::Full());
      break;
  }
  // Finally, we expand the bound to account for the error when a point P is
  // converted to an S2LatLng to test for containment.  (The bound should be
  // large enough so that it contains the computed S2LatLng of any contained
  // point, not just the infinite-precision version.)  We don't need to expand
  // longitude because longitude is calculated via a single call to atan2(),
  // which is guaranteed to be semi-monotonic.  (In fact the Gnu implementation
  // is also correctly rounded, but we don't even need that here.)
  return bound.Expanded(S2LatLng::FromRadians(DBL_EPSILON, 0));
}

bool S2Cell::MayIntersect(const S2Cell& cell) const {
  return id_.intersects(cell.id_);
}

bool S2Cell::Contains(const S2Cell& cell) const {
  return id_.contains(cell.id_);
}

bool S2Cell::Contains(const S2Point& p) const {
  // We can't just call XYZtoFaceUV, because for points that lie on the
  // boundary between two faces (i.e. u or v is +1/-1) we need to return
  // true for both adjacent cells.
  R2Point uv;
  if (!S2::FaceXYZtoUV(face_, p, &uv)) return false;

  // Expand the (u,v) bound to ensure that
  //
  //   S2Cell(S2CellId(p)).Contains(p)
  //
  // is always true.  To do this, we need to account for the error when
  // converting from (u,v) coordinates to (s,t) coordinates.  At least in the
  // case of S2_QUADRATIC_PROJECTION, the total error is at most DBL_EPSILON.
  return uv_.Expanded(DBL_EPSILON).Contains(uv);
}

void S2Cell::Encode(Encoder* const encoder) const {
  id_.Encode(encoder);
}

bool S2Cell::Decode(Decoder* const decoder) {
  S2CellId id;
  if (!id.Decode(decoder)) return false;
  this->~S2Cell();
  new (this) S2Cell(id);
  return true;
}

// Return the squared chord distance from point P to corner vertex (i,j).
inline S1ChordAngle S2Cell::VertexChordDist(
    const S2Point& p, int i, int j) const {
  S2Point vertex = S2Point(uv_[0][i], uv_[1][j], 1).Normalize();
  return S1ChordAngle(p, vertex);
}

// Given a point P and either the lower or upper edge of the S2Cell (specified
// by setting "v_end" to 0 or 1 respectively), return true if P is closer to
// the interior of that edge than it is to either endpoint.
bool S2Cell::UEdgeIsClosest(const S2Point& p, int v_end) const {
  double u0 = uv_[0][0], u1 = uv_[0][1], v = uv_[1][v_end];
  // These are the normals to the planes that are perpendicular to the edge
  // and pass through one of its two endpoints.
  S2Point dir0(v * v + 1, -u0 * v, -u0);
  S2Point dir1(v * v + 1, -u1 * v, -u1);
  return p.DotProd(dir0) > 0 && p.DotProd(dir1) < 0;
}

// Given a point P and either the left or right edge of the S2Cell (specified
// by setting "u_end" to 0 or 1 respectively), return true if P is closer to
// the interior of that edge than it is to either endpoint.
bool S2Cell::VEdgeIsClosest(const S2Point& p, int u_end) const {
  double v0 = uv_[1][0], v1 = uv_[1][1], u = uv_[0][u_end];
  // See comments above.
  S2Point dir0(-u * v0, u * u + 1, -v0);
  S2Point dir1(-u * v1, u * u + 1, -v1);
  return p.DotProd(dir0) > 0 && p.DotProd(dir1) < 0;
}

// Given the dot product of a point P with the normal of a u- or v-edge at the
// given coordinate value, return the distance from P to that edge.
inline static S1ChordAngle EdgeDistance(double dirIJ, double uv) {
  // Let P by the target point and let R be the closest point on the given
  // edge AB.  The desired distance PR can be expressed as PR^2 = PQ^2 + QR^2
  // where Q is the point P projected onto the plane through the great circle
  // through AB.  We can compute the distance PQ^2 perpendicular to the plane
  // from "dirIJ" (the dot product of the target point P with the edge
  // normal) and the squared length the edge normal (1 + uv**2).
  double pq2 = (dirIJ * dirIJ) / (1 + uv * uv);

  // We can compute the distance QR as (1 - OQ) where O is the sphere origin,
  // and we can compute OQ^2 = 1 - PQ^2 using the Pythagorean theorem.
  // (This calculation loses accuracy as angle POQ approaches Pi/2.)
  double qr = 1 - sqrt(1 - pq2);
  return S1ChordAngle::FromLength2(pq2 + qr * qr);
}

S1ChordAngle S2Cell::GetDistanceInternal(const S2Point& target_xyz,
                                         bool to_interior) const {
  // All calculations are done in the (u,v,w) coordinates of this cell's face.
  S2Point target = S2::FaceXYZtoUVW(face_, target_xyz);

  // Compute dot products with all four upward or rightward-facing edge
  // normals.  "dirIJ" is the dot product for the edge corresponding to axis
  // I, endpoint J.  For example, dir01 is the right edge of the S2Cell
  // (corresponding to the upper endpoint of the u-axis).
  double dir00 = target[0] - target[2] * uv_[0][0];
  double dir01 = target[0] - target[2] * uv_[0][1];
  double dir10 = target[1] - target[2] * uv_[1][0];
  double dir11 = target[1] - target[2] * uv_[1][1];
  bool inside = true;
  if (dir00 < 0) {
    inside = false;  // Target is to the left of the cell
    if (VEdgeIsClosest(target, 0)) return EdgeDistance(-dir00, uv_[0][0]);
  }
  if (dir01 > 0) {
    inside = false;  // Target is to the right of the cell
    if (VEdgeIsClosest(target, 1)) return EdgeDistance(dir01, uv_[0][1]);
  }
  if (dir10 < 0) {
    inside = false;  // Target is below the cell
    if (UEdgeIsClosest(target, 0)) return EdgeDistance(-dir10, uv_[1][0]);
  }
  if (dir11 > 0) {
    inside = false;  // Target is above the cell
    if (UEdgeIsClosest(target, 1)) return EdgeDistance(dir11, uv_[1][1]);
  }
  if (inside) {
    if (to_interior) return S1ChordAngle::Zero();
    // Although you might think of S2Cells as rectangles, they are actually
    // arbitrary quadrilaterals after they are projected onto the sphere.
    // Therefore the simplest approach is just to find the minimum distance to
    // any of the four edges.
    return min(min(EdgeDistance(-dir00, uv_[0][0]),
                   EdgeDistance(dir01, uv_[0][1])),
               min(EdgeDistance(-dir10, uv_[1][0]),
                   EdgeDistance(dir11, uv_[1][1])));
  }
  // Otherwise, the closest point is one of the four cell vertices.  Note that
  // it is *not* trivial to narrow down the candidates based on the edge sign
  // tests above, because (1) the edges don't meet at right angles and (2)
  // there are points on the far side of the sphere that are both above *and*
  // below the cell, etc.
  return min(min(VertexChordDist(target, 0, 0),
                 VertexChordDist(target, 1, 0)),
             min(VertexChordDist(target, 0, 1),
                 VertexChordDist(target, 1, 1)));
}

S1ChordAngle S2Cell::GetDistance(const S2Point& target) const {
  return GetDistanceInternal(target, true /*to_interior*/);
}

S1ChordAngle S2Cell::GetBoundaryDistance(const S2Point& target) const {
  return GetDistanceInternal(target, false /*to_interior*/);
}

S1ChordAngle S2Cell::GetMaxDistance(const S2Point& target) const {
  // First check the 4 cell vertices.  If all are within the hemisphere
  // centered around target, the max distance will be to one of these vertices.
  S2Point target_uvw = S2::FaceXYZtoUVW(face_, target);
  S1ChordAngle max_dist = max(max(VertexChordDist(target_uvw, 0, 0),
                                         VertexChordDist(target_uvw, 1, 0)),
                                     max(VertexChordDist(target_uvw, 0, 1),
                                         VertexChordDist(target_uvw, 1, 1)));

  if (max_dist <= S1ChordAngle::Right()) {
    return max_dist;
  }

  // Otherwise, find the minimum distance d_min to the antipodal point and the
  // maximum distance will be Pi - d_min.
  return S1ChordAngle::Straight() - GetDistance(-target);
}

S1ChordAngle S2Cell::GetDistance(const S2Point& a, const S2Point& b) const {
  // Possible optimizations:
  //  - Currently the (cell vertex, edge endpoint) distances are computed
  //    twice each, and the length of AB is computed 4 times.
  //  - To fix this, refactor GetDistance(target) so that it skips calculating
  //    the distance to each cell vertex.  Instead, compute the cell vertices
  //    and distances in this function, and add a low-level UpdateMinDistance
  //    that allows the XA, XB, and AB distances to be passed in.
  //  - It might also be more efficient to do all calculations in UVW-space,
  //    since this would involve transforming 2 points rather than 4.

  // First, check the minimum distance to the edge endpoints A and B.
  // (This also detects whether either endpoint is inside the cell.)
  S1ChordAngle min_dist = min(GetDistance(a), GetDistance(b));
  if (min_dist == S1ChordAngle::Zero()) return min_dist;

  // Otherwise, check whether the edge crosses the cell boundary.
  // Note that S2EdgeCrosser needs pointers to vertices.
  S2Point v[4];
  for (int i = 0; i < 4; ++i) {
    v[i] = GetVertex(i);
  }
  S2EdgeCrosser crosser(&a, &b, &v[3]);
  for (int i = 0; i < 4; ++i) {
    if (crosser.CrossingSign(&v[i]) >= 0) {
      return S1ChordAngle::Zero();
    }
  }
  // Finally, check whether the minimum distance occurs between a cell vertex
  // and the interior of the edge AB.  (Some of this work is redundant, since
  // it also checks the distance to the endpoints A and B again.)
  //
  // Note that we don't need to check the distance from the interior of AB to
  // the interior of a cell edge, because the only way that this distance can
  // be minimal is if the two edges cross (already checked above).
  for (int i = 0; i < 4; ++i) {
    S2::UpdateMinDistance(v[i], a, b, &min_dist);
  }
  return min_dist;
}

S1ChordAngle S2Cell::GetMaxDistance(const S2Point& a, const S2Point& b) const {
  // If the maximum distance from both endpoints to the cell is less than Pi/2
  // then the maximum distance from the edge to the cell is the maximum of the
  // two endpoint distances.
  S1ChordAngle max_dist = max(GetMaxDistance(a), GetMaxDistance(b));
  if (max_dist <= S1ChordAngle::Right()) {
    return max_dist;
  }

  return S1ChordAngle::Straight() - GetDistance(-a, -b);
}

S1ChordAngle S2Cell::GetDistance(const S2Cell& target) const {
  // If the cells intersect, the distance is zero.  We use the (u,v) ranges
  // rather S2CellId::intersects() so that cells that share a partial edge or
  // corner are considered to intersect.
  if (face_ == target.face_ && uv_.Intersects(target.uv_)) {
    return S1ChordAngle::Zero();
  }

  // Otherwise, the minimum distance always occurs between a vertex of one
  // cell and an edge of the other cell (including the edge endpoints).  This
  // represents a total of 32 possible (vertex, edge) pairs.
  //
  // TODO(ericv): This could be optimized to be at least 5x faster by pruning
  // the set of possible closest vertex/edge pairs using the faces and (u,v)
  // ranges of both cells.
  S2Point va[4], vb[4];
  for (int i = 0; i < 4; ++i) {
    va[i] = GetVertex(i);
    vb[i] = target.GetVertex(i);
  }
  S1ChordAngle min_dist = S1ChordAngle::Infinity();
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      S2::UpdateMinDistance(va[i], vb[j], vb[(j + 1) & 3], &min_dist);
      S2::UpdateMinDistance(vb[i], va[j], va[(j + 1) & 3], &min_dist);
    }
  }
  return min_dist;
}

inline static int OppositeFace(int face) {
  return face >= 3 ? face - 3 : face + 3;
}

// The antipodal UV is the transpose of the original UV, interpreted within
// the opposite face.
inline static R2Rect OppositeUV(const R2Rect& uv) {
  return R2Rect(uv[1], uv[0]);
}

S1ChordAngle S2Cell::GetMaxDistance(const S2Cell& target) const {
  // Need to check the antipodal target for intersection with the cell. If it
  // intersects, the distance is S1ChordAngle::Straight().
  if (face_ == OppositeFace(target.face_) &&
      uv_.Intersects(OppositeUV(target.uv_))) {
    return S1ChordAngle::Straight();
  }

  // Otherwise, the maximum distance always occurs between a vertex of one
  // cell and an edge of the other cell (including the edge endpoints).  This
  // represents a total of 32 possible (vertex, edge) pairs.
  //
  // TODO(user): When the maximum distance is at most Pi/2, the maximum is
  // always attained between a pair of vertices, and this could be made much
  // faster by testing each vertex pair once rather than the current 4 times.
  S2Point va[4], vb[4];
  for (int i = 0; i < 4; ++i) {
    va[i] = GetVertex(i);
    vb[i] = target.GetVertex(i);
  }
  S1ChordAngle max_dist = S1ChordAngle::Negative();
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      S2::UpdateMaxDistance(va[i], vb[j], vb[(j + 1) & 3], &max_dist);
      S2::UpdateMaxDistance(vb[i], va[j], va[(j + 1) & 3], &max_dist);
    }
  }
  return max_dist;
}

