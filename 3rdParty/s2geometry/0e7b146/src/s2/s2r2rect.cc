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

#include "s2/s2r2rect.h"

#include <iosfwd>

#include "s2/base/logging.h"
#include "s2/r1interval.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2coords.h"
#include "s2/s2latlng_rect.h"

S2R2Rect S2R2Rect::FromCell(const S2Cell& cell) {
  // S2Cells have a more efficient GetSizeST() method than S2CellIds.
  double size = cell.GetSizeST();
  return FromCenterSize(cell.id().GetCenterST(), R2Point(size, size));
}

S2R2Rect S2R2Rect::FromCellId(S2CellId id) {
  double size = id.GetSizeST();
  return FromCenterSize(id.GetCenterST(), R2Point(size, size));
}

S2R2Rect* S2R2Rect::Clone() const {
  return new S2R2Rect(*this);
}

S2Point S2R2Rect::ToS2Point(const R2Point& p) {
  return S2::FaceUVtoXYZ(0, S2::STtoUV(p.x()), S2::STtoUV(p.y())).Normalize();
}

S2Cap S2R2Rect::GetCapBound() const {
  if (is_empty()) return S2Cap::Empty();

  // The rectangle is a convex polygon on the sphere, since it is a subset of
  // one cube face.  Its bounding cap is also a convex region on the sphere,
  // and therefore we can bound the rectangle by just bounding its vertices.
  // We use the rectangle's center in (s,t)-space as the cap axis.  This
  // doesn't yield the minimal cap but it's pretty close.
  S2Cap cap = S2Cap::FromPoint(ToS2Point(GetCenter()));
  for (int k = 0; k < 4; ++k) {
    cap.AddPoint(ToS2Point(GetVertex(k)));
  }
  return cap;
}

S2LatLngRect S2R2Rect::GetRectBound() const {
  // This is not very tight but hopefully good enough.
  return GetCapBound().GetRectBound();
}

bool S2R2Rect::Contains(const S2Point& p) const {
  if (S2::GetFace(p) != 0) return false;
  double u, v;
  S2::ValidFaceXYZtoUV(0, p, &u, &v);
  return Contains(R2Point(S2::UVtoST(u), S2::UVtoST(v)));
}

bool S2R2Rect::Contains(const S2Cell& cell) const {
  if (cell.face() != 0) return false;
  return Contains(S2R2Rect::FromCell(cell));
}

bool S2R2Rect::MayIntersect(const S2Cell& cell) const {
  if (cell.face() != 0) return false;
  return Intersects(S2R2Rect::FromCell(cell));
}

std::ostream& operator<<(std::ostream& os, const S2R2Rect& r) {
  return os << "[Lo" << r.lo() << ", Hi" << r.hi() << "]";
}
