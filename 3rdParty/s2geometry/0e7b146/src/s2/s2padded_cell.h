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

#ifndef S2_S2PADDED_CELL_H_
#define S2_S2PADDED_CELL_H_

#include "s2/_fp_contract_off.h"
#include "s2/r2rect.h"
#include "s2/s2cell_id.h"

// S2PaddedCell represents an S2Cell whose (u,v)-range has been expanded on
// all sides by a given amount of "padding".  Unlike S2Cell, its methods and
// representation are optimized for clipping edges against S2Cell boundaries
// to determine which cells are intersected by a given set of edges.
//
// This class is intended to be copied by value as desired.
class S2PaddedCell {
 public:
  // Construct an S2PaddedCell for the given cell id and padding.
  S2PaddedCell(S2CellId id, double padding);

  // Construct the child of "parent" with the given (i,j) index.  The four
  // child cells have indices of (0,0), (0,1), (1,0), (1,1), where the i and j
  // indices correspond to increasing u- and v-values respectively.
  S2PaddedCell(const S2PaddedCell& parent, int i, int j);

  S2CellId id() const { return id_; }
  double padding() const { return padding_; }
  int level() const { return level_; }

  // Return the bound for this cell (including padding).
  const R2Rect& bound() const { return bound_; }

  // Return the "middle" of the padded cell, defined as the rectangle that
  // belongs to all four children.
  //
  // Note that this method is *not* thread-safe, because the return value is
  // computed on demand and cached.  (It is expected that this class will be
  // mainly useful in the context of single-threaded recursive algorithms.)
  const R2Rect& middle() const;

  // Return the (i,j) coordinates for the child cell at the given traversal
  // position.  The traversal position corresponds to the order in which child
  // cells are visited by the Hilbert curve.
  void GetChildIJ(int pos, int* i, int* j) const;

  // Return the smallest cell that contains all descendants of this cell whose
  // bounds intersect "rect".  For algorithms that use recursive subdivision
  // to find the cells that intersect a particular object, this method can be
  // used to skip all the initial subdivision steps where only one child needs
  // to be expanded.
  //
  // Note that this method is not the same as returning the smallest cell that
  // contains the intersection of this cell with "rect".  Because of the
  // padding, even if one child completely contains "rect" it is still
  // possible that a neighboring child also intersects "rect".
  //
  // REQUIRES: bound().Intersects(rect)
  S2CellId ShrinkToFit(const R2Rect& rect) const;

  // Return the center of this cell.
  S2Point GetCenter() const;

  // Return the vertex where the S2 space-filling curve enters this cell.
  S2Point GetEntryVertex() const;

  // Return the vertex where the S2 space-filling curve exits this cell.
  S2Point GetExitVertex() const;

 private:
  S2CellId id_;
  double padding_;
  R2Rect bound_;     // Bound in (u,v)-space

  // The rectangle in (u,v)-space that belongs to all four padded children.
  // It is computed on demand by the middle() accessor method.
  mutable R2Rect middle_;

  int ij_lo_[2];     // Minimum (i,j)-coordinates of this cell, before padding
  int orientation_;  // Hilbert curve orientation of this cell (see s2coords.h)
  int level_;        // Level of this cell (see s2coords.h)
};


//////////////////   Implementation details follow   ////////////////////


inline void S2PaddedCell::GetChildIJ(int pos, int* i, int* j) const {
  int ij = S2::internal::kPosToIJ[orientation_][pos];
  *i = ij >> 1;
  *j = ij & 1;
}

#endif  // S2_S2PADDED_CELL_H_
