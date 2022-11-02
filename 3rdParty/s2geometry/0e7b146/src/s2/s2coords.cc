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

#include "s2/s2coords.h"

#include "s2/util/bits/bits.h"

namespace S2 {

namespace internal {

// Define the "extern" constants in s2coords_internal.h.

static_assert(kSwapMask == 0x01 && kInvertMask == 0x02, "masks changed");

// kIJtoPos[orientation][ij] -> pos
const int kIJtoPos[4][4] = {
  // (0,0) (0,1) (1,0) (1,1)
  {     0,    1,    3,    2  },  // canonical order
  {     0,    3,    1,    2  },  // axes swapped
  {     2,    3,    1,    0  },  // bits inverted
  {     2,    1,    3,    0  },  // swapped & inverted
};

// kPosToIJ[orientation][pos] -> ij
const int kPosToIJ[4][4] = {
  // 0  1  2  3
  {  0, 1, 3, 2 },    // canonical order:    (0,0), (0,1), (1,1), (1,0)
  {  0, 2, 3, 1 },    // axes swapped:       (0,0), (1,0), (1,1), (0,1)
  {  3, 2, 0, 1 },    // bits inverted:      (1,1), (1,0), (0,0), (0,1)
  {  3, 1, 0, 2 },    // swapped & inverted: (1,1), (0,1), (0,0), (1,0)
};

// kPosToOrientation[pos] -> orientation_modifier
const int kPosToOrientation[4] = {
  kSwapMask,
  0,
  0,
  kInvertMask + kSwapMask,
};

const int kFaceUVWFaces[6][3][2] = {
  { { 4, 1 }, { 5, 2 }, { 3, 0 } },
  { { 0, 3 }, { 5, 2 }, { 4, 1 } },
  { { 0, 3 }, { 1, 4 }, { 5, 2 } },
  { { 2, 5 }, { 1, 4 }, { 0, 3 } },
  { { 2, 5 }, { 3, 0 }, { 1, 4 } },
  { { 4, 1 }, { 3, 0 }, { 2, 5 } }
};

const double kFaceUVWAxes[6][3][3] = {
  {
    { 0,  1,  0 },
    { 0,  0,  1 },
    { 1,  0,  0 }
  },
  {
    {-1,  0,  0 },
    { 0,  0,  1 },
    { 0,  1,  0 }
  },
  {
    {-1,  0,  0 },
    { 0, -1,  0 },
    { 0,  0,  1 }
  },
  {
    { 0,  0, -1 },
    { 0, -1,  0 },
    {-1,  0,  0 }
  },
  {
    { 0,  0, -1 },
    { 1,  0,  0 },
    { 0, -1,  0 }
  },
  {
    { 0,  1,  0 },
    { 1,  0,  0 },
    { 0,  0, -1 }
  }
};

} // namespace internal



S2Point FaceXYZtoUVW(int face, const S2Point& p) {
  // The result coordinates are simply the dot products of P with the (u,v,w)
  // axes for the given face (see kFaceUVWAxes).
  switch (face) {
    case 0:  return S2Point( p.y(),  p.z(),  p.x());
    case 1:  return S2Point(-p.x(),  p.z(),  p.y());
    case 2:  return S2Point(-p.x(), -p.y(),  p.z());
    case 3:  return S2Point(-p.z(), -p.y(), -p.x());
    case 4:  return S2Point(-p.z(),  p.x(), -p.y());
    default: return S2Point( p.y(),  p.x(), -p.z());
  }
}

int XYZtoFaceSiTi(const S2Point& p, int* face, unsigned int* si,
                  unsigned int* ti) {
  double u, v;
  *face = XYZtoFaceUV(p, &u, &v);
  *si = STtoSiTi(UVtoST(u));
  *ti = STtoSiTi(UVtoST(v));
  // If the levels corresponding to si,ti are not equal, then p is not a cell
  // center.  The si,ti values 0 and kMaxSiTi need to be handled specially
  // because they do not correspond to cell centers at any valid level; they
  // are mapped to level -1 by the code below.
  int level = kMaxCellLevel - Bits::FindLSBSetNonZero(*si | kMaxSiTi);
  if (level < 0 ||
      level != kMaxCellLevel - Bits::FindLSBSetNonZero(*ti | kMaxSiTi)) {
    return -1;
  }
  S2_DCHECK_LE(level, kMaxCellLevel);
  // In infinite precision, this test could be changed to ST == SiTi. However,
  // due to rounding errors, UVtoST(XYZtoFaceUV(FaceUVtoXYZ(STtoUV(...)))) is
  // not idempotent. On the other hand, center_raw is computed exactly the same
  // way p was originally computed (if it is indeed the center of an S2Cell):
  // the comparison can be exact.
  S2Point center = FaceSiTitoXYZ(*face, *si, *ti).Normalize();
  return p == center ? level : -1;
}

S2Point FaceSiTitoXYZ(int face, unsigned int si, unsigned int ti) {
  double u = STtoUV(SiTitoST(si));
  double v = STtoUV(SiTitoST(ti));
  return FaceUVtoXYZ(face, u, v);
}

}  // namespace S2
