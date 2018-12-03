// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef S2_ENCODED_S2CELL_ID_VECTOR_H_
#define S2_ENCODED_S2CELL_ID_VECTOR_H_

#include "s2/third_party/absl/types/span.h"
#include "s2/encoded_uint_vector.h"
#include "s2/s2cell_id.h"

namespace s2coding {

// Encodes a vector of S2CellIds in a format that can later be decoded as an
// EncodedS2CellIdVector.  The S2CellIds do not need to be valid.
//
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
void EncodeS2CellIdVector(absl::Span<const S2CellId> v, Encoder* encoder);

// This class represents an encoded vector of S2CellIds.  Values are decoded
// only when they are accessed.  This allows for very fast initialization and
// no additional memory use beyond the encoded data.  The encoded data is not
// owned by this class; typically it points into a large contiguous buffer
// that contains other encoded data as well.
//
// This is one of several helper classes that allow complex data structures to
// be initialized from an encoded format in constant time and then decoded on
// demand.  This can be a big performance advantage when only a small part of
// the data structure is actually used.
//
// The S2CellIds do not need to be sorted or at the same level.  The
// implementation is biased towards minimizing decoding times rather than
// space usage.
//
// NOTE: If your S2CellIds represent S2Points that have been snapped to
// S2CellId centers, then EncodedS2PointVector is both faster and smaller.
class EncodedS2CellIdVector {
 public:
  // Constructs an uninitialized object; requires Init() to be called.
  EncodedS2CellIdVector() {}

  // Initializes the EncodedS2CellIdVector.
  //
  // REQUIRES: The Decoder data buffer must outlive this object.
  bool Init(Decoder* decoder);

  // Returns the size of the original vector.
  size_t size() const;

  // Returns the element at the given index.
  S2CellId operator[](int i) const;

  // Returns the index of the first element x such that (x >= target), or
  // size() if no such element exists.
  //
  // REQUIRES: The vector elements are sorted in non-decreasing order.
  size_t lower_bound(S2CellId target) const;

  // Decodes and returns the entire original vector.
  std::vector<S2CellId> Decode() const;

 private:
  // Values are decoded as (base_ + (deltas_[i] << shift_)).
  EncodedUintVector<uint64> deltas_;
  uint64 base_;
  uint8 shift_;
};


//////////////////   Implementation details follow   ////////////////////


inline size_t EncodedS2CellIdVector::size() const {
  return deltas_.size();
}

inline S2CellId EncodedS2CellIdVector::operator[](int i) const {
  return S2CellId((deltas_[i] << shift_) + base_);
}

inline size_t EncodedS2CellIdVector::lower_bound(S2CellId target) const {
  // We optimize the search by converting "target" to the corresponding delta
  // value and then searching directly in the deltas_ vector.
  //
  // To invert operator[], we essentially compute ((target - base_) >> shift_)
  // except that we also need to round up when shifting.  The first two cases
  // ensure that "target" doesn't wrap around past zero when we do this.
  if (target.id() <= base_) return 0;
  if (target >= S2CellId::End(S2CellId::kMaxLevel)) return size();
  return deltas_.lower_bound(
      (target.id() - base_ + (1ULL << shift_) - 1) >> shift_);
}

}  // namespace s2coding

#endif  // S2_ENCODED_S2CELL_ID_VECTOR_H_
