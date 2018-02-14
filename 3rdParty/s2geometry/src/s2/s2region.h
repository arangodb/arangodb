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

#ifndef S2_S2REGION_H_
#define S2_S2REGION_H_

#include <vector>

#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"

class Decoder;
class Encoder;

class S2Cap;
class S2Cell;
class S2CellId;
class S2LatLngRect;

// An S2Region represents a two-dimensional region over the unit sphere.
// It is an abstract interface with various concrete subtypes.
//
// The main purpose of this interface is to allow complex regions to be
// approximated as simpler regions.  So rather than having a wide variety
// of virtual methods that are implemented by all subtypes, the interface
// is restricted to methods that are useful for computing approximations.
class S2Region {
 public:
  virtual ~S2Region() {}

  // Returns a deep copy of the region.
  //
  // Note that each subtype of S2Region returns a pointer to an object of its
  // own type (e.g., S2Cap::Clone() returns an S2Cap*).
  virtual S2Region* Clone() const = 0;

  // Returns a bounding spherical cap that contains the region.  The bound may
  // not be tight.
  virtual S2Cap GetCapBound() const = 0;

  // Returns a bounding latitude-longitude rectangle that contains the region.
  // The bound may not be tight.
  virtual S2LatLngRect GetRectBound() const = 0;

  // Returns a small collection of S2CellIds whose union covers the region.
  // The cells are not sorted, may have redundancies (such as cells that
  // contain other cells), and may cover much more area than necessary.
  //
  // This method is not intended for direct use by client code.  Clients
  // should typically use S2RegionCoverer::GetCovering, which has options to
  // control the size and accuracy of the covering.  Alternatively, if you
  // want a fast covering and don't care about accuracy, consider calling
  // S2RegionCoverer::GetFastCovering (which returns a cleaned-up version of
  // the covering computed by this method).
  //
  // GetCellUnionBound() implementations should attempt to return a small
  // covering (ideally 4 cells or fewer) that covers the region and can be
  // computed quickly.  The result is used by S2RegionCoverer as a starting
  // point for further refinement.
  //
  // TODO(ericv): Remove the default implementation.
  virtual void GetCellUnionBound(std::vector<S2CellId> *cell_ids) const;

  // Returns true if the region completely contains the given cell, otherwise
  // returns false.
  virtual bool Contains(const S2Cell& cell) const = 0;

  // If this method returns false, the region does not intersect the given
  // cell.  Otherwise, either region intersects the cell, or the intersection
  // relationship could not be determined.
  //
  // Note that there is currently exactly one implementation of this method
  // (S2LatLngRect::MayIntersect) that takes advantage of the semantics above
  // to be more efficient.  For all other S2Region subtypes, this method
  // returns true if the region intersect the cell and false otherwise.
  virtual bool MayIntersect(const S2Cell& cell) const = 0;

  // Returns true if and only if the given point is contained by the region.
  // The point 'p' is generally required to be unit length, although some
  // subtypes may relax this restriction.
  virtual bool Contains(const S2Point& p) const = 0;

  //////////////////////////////////////////////////////////////////////////
  // Many S2Region subtypes also define the following non-virtual methods.
  //////////////////////////////////////////////////////////////////////////

  // Appends a serialized representation of the region to "encoder".
  //
  // The representation chosen is left up to the sub-classes but it should
  // satisfy the following constraints:
  // - It should encode a version number.
  // - It should be deserializable using the corresponding Decode method.
  // - Performance, not space, should be the chief consideration. Encode() and
  //   Decode() should be implemented such that the combination is equivalent
  //   to calling Clone().
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  //
  // void Encode(Encoder* const encoder) const;

  // Decodes an S2Region encoded with Encode().  Note that this method
  // requires that an S2Region object of the appropriate concrete type has
  // already been constructed.  It is not possible to decode regions of
  // unknown type.
  //
  // Whenever the Decode method is changed to deal with new serialized
  // representations, it should be done so in a manner that allows for
  // older versions to be decoded i.e. the version number in the serialized
  // representation should be used to decide how to decode the data.
  //
  // Returns true on success.
  //
  // bool Decode(Decoder* const decoder);

  // Provides the same functionality as Decode, except that decoded regions
  // are allowed to point directly into the Decoder's memory buffer rather
  // than copying the data.  This method can be much faster for regions that
  // have a lot of data (such as polygons), but the decoded region is only
  // valid within the scope (lifetime) of the Decoder's memory buffer.
  //
  // bool DecodeWithinScope(Decoder* const decoder);
};

#endif  // S2_S2REGION_H_
