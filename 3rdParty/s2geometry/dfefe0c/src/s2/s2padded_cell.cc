// Copyright 2013 Google Inc. All Rights Reserved.
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

#include "s2/s2padded_cell.h"

#include <algorithm>
#include <cfloat>

#include "s2/util/bits/bits.h"
#include "s2/r1interval.h"
#include "s2/s2coords.h"

using std::max;
using std::min;
using S2::internal::kSwapMask;
using S2::internal::kInvertMask;
using S2::internal::kIJtoPos;
using S2::internal::kPosToOrientation;

S2PaddedCell::S2PaddedCell(S2CellId id, double padding)
    : id_(id), padding_(padding) {
  if (id_.is_face()) {
    // Fast path for constructing a top-level face (the most common case).
    double limit = 1 + padding;
    bound_ = R2Rect(R1Interval(-limit, limit), R1Interval(-limit, limit));
    middle_ = R2Rect(R1Interval(-padding, padding),
                     R1Interval(-padding, padding));
    ij_lo_[0] = ij_lo_[1] = 0;
    orientation_ = id_.face() & 1;
    level_ = 0;
  } else {
    int ij[2];
    id.ToFaceIJOrientation(&ij[0], &ij[1], &orientation_);
    level_ = id.level();
    bound_ = S2CellId::IJLevelToBoundUV(ij, level_).Expanded(padding);
    int ij_size = S2CellId::GetSizeIJ(level_);
    ij_lo_[0] = ij[0] & -ij_size;
    ij_lo_[1] = ij[1] & -ij_size;
  }
}

S2PaddedCell::S2PaddedCell(const S2PaddedCell& parent, int i, int j)
    : padding_(parent.padding_),
      bound_(parent.bound_),
      level_(parent.level_ + 1) {
  // Compute the position and orientation of the child incrementally from the
  // orientation of the parent.
  int pos = kIJtoPos[parent.orientation_][2*i+j];
  id_ = parent.id_.child(pos);
  int ij_size = S2CellId::GetSizeIJ(level_);
  ij_lo_[0] = parent.ij_lo_[0] + i * ij_size;
  ij_lo_[1] = parent.ij_lo_[1] + j * ij_size;
  orientation_ = parent.orientation_ ^ kPosToOrientation[pos];
  // For each child, one corner of the bound is taken directly from the parent
  // while the diagonally opposite corner is taken from middle().
  const R2Rect& middle = parent.middle();
  bound_[0][1-i] = middle[0][1-i];
  bound_[1][1-j] = middle[1][1-j];
}

const R2Rect& S2PaddedCell::middle() const {
  // We compute this field lazily because it is not needed the majority of the
  // time (i.e., for cells where the recursion terminates).
  if (middle_.is_empty()) {
    int ij_size = S2CellId::GetSizeIJ(level_);
    double u = S2::STtoUV(S2::SiTitoST(2 * ij_lo_[0] + ij_size));
    double v = S2::STtoUV(S2::SiTitoST(2 * ij_lo_[1] + ij_size));
    middle_ = R2Rect(R1Interval(u - padding_, u + padding_),
                     R1Interval(v - padding_, v + padding_));
  }
  return middle_;
}

S2Point S2PaddedCell::GetCenter() const {
  int ij_size = S2CellId::GetSizeIJ(level_);
  unsigned int si = 2 * ij_lo_[0] + ij_size;
  unsigned int ti = 2 * ij_lo_[1] + ij_size;
  return S2::FaceSiTitoXYZ(id_.face(), si, ti).Normalize();
}

S2Point S2PaddedCell::GetEntryVertex() const {
  // The curve enters at the (0,0) vertex unless the axis directions are
  // reversed, in which case it enters at the (1,1) vertex.
  unsigned int i = ij_lo_[0];
  unsigned int j = ij_lo_[1];
  if (orientation_ & kInvertMask) {
    int ij_size = S2CellId::GetSizeIJ(level_);
    i += ij_size;
    j += ij_size;
  }
  return S2::FaceSiTitoXYZ(id_.face(), 2 * i, 2 * j).Normalize();
}

S2Point S2PaddedCell::GetExitVertex() const {
  // The curve exits at the (1,0) vertex unless the axes are swapped or
  // inverted but not both, in which case it exits at the (0,1) vertex.
  unsigned int i = ij_lo_[0];
  unsigned int j = ij_lo_[1];
  int ij_size = S2CellId::GetSizeIJ(level_);
  if (orientation_ == 0 || orientation_ == kSwapMask + kInvertMask) {
    i += ij_size;
  } else {
    j += ij_size;
  }
  return S2::FaceSiTitoXYZ(id_.face(), 2 * i, 2 * j).Normalize();
}

S2CellId S2PaddedCell::ShrinkToFit(const R2Rect& rect) const {
  S2_DCHECK(bound().Intersects(rect));

  // Quick rejection test: if "rect" contains the center of this cell along
  // either axis, then no further shrinking is possible.
  int ij_size = S2CellId::GetSizeIJ(level_);
  if (level_ == 0) {
    // Fast path (most calls to this function start with a face cell).
    if (rect[0].Contains(0) || rect[1].Contains(0)) return id();
  } else {
    if (rect[0].Contains(S2::STtoUV(S2::SiTitoST(2 * ij_lo_[0] + ij_size))) ||
        rect[1].Contains(S2::STtoUV(S2::SiTitoST(2 * ij_lo_[1] + ij_size)))) {
      return id();
    }
  }
  // Otherwise we expand "rect" by the given padding() on all sides and find
  // the range of coordinates that it spans along the i- and j-axes.  We then
  // compute the highest bit position at which the min and max coordinates
  // differ.  This corresponds to the first cell level at which at least two
  // children intersect "rect".

  // Increase the padding to compensate for the error in S2::UVtoST().
  // (The constant below is a provable upper bound on the additional error.)
  R2Rect padded = rect.Expanded(padding() + 1.5 * DBL_EPSILON);
  int ij_min[2];  // Min i- or j- coordinate spanned by "padded"
  int ij_xor[2];  // XOR of the min and max i- or j-coordinates
  for (int d = 0; d < 2; ++d) {
    ij_min[d] = max(ij_lo_[d], S2::STtoIJ(S2::UVtoST(padded[d][0])));
    int ij_max = min(ij_lo_[d] + ij_size - 1,
                     S2::STtoIJ(S2::UVtoST(padded[d][1])));
    ij_xor[d] = ij_min[d] ^ ij_max;
  }
  // Compute the highest bit position where the two i- or j-endpoints differ,
  // and then choose the cell level that includes both of these endpoints.  So
  // if both pairs of endpoints are equal we choose kMaxLevel; if they differ
  // only at bit 0, we choose (kMaxLevel - 1), and so on.
  int level_msb = ((ij_xor[0] | ij_xor[1]) << 1) + 1;
  int level = S2CellId::kMaxLevel - Bits::Log2FloorNonZero(level_msb);
  if (level <= level_) return id();
  return S2CellId::FromFaceIJ(id().face(), ij_min[0], ij_min[1]).parent(level);
}
