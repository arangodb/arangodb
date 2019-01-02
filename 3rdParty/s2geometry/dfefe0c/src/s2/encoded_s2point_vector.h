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

#ifndef S2_ENCODED_S2POINT_VECTOR_H_
#define S2_ENCODED_S2POINT_VECTOR_H_

#include "s2/third_party/absl/types/span.h"
#include "s2/encoded_uint_vector.h"
#include "s2/s2point.h"

namespace s2coding {

// Controls whether to optimize for speed or size when encoding points.  (Note
// that encoding is always lossless, and that currently compact encodings are
// only possible when points have been snapped to S2CellId centers.)
enum CodingHint { FAST, COMPACT };

// Encodes a vector of S2Points in a format that can later be decoded as an
// EncodedS2PointVector.
//
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
void EncodeS2PointVector(absl::Span<const S2Point> v, CodingHint hint,
                         Encoder* encoder);

// This class represents an encoded vector of S2Points.  Values are decoded
// only when they are accessed.  This allows for very fast initialization and
// no additional memory use beyond the encoded data.  The encoded data is not
// owned by this class; typically it points into a large contiguous buffer
// that contains other encoded data as well.
//
// This is one of several helper classes that allow complex data structures to
// be initialized from an encoded format in constant time and then decoded on
// demand.  This can be a big performance advantage when only a small part of
// the data structure is actually used.
class EncodedS2PointVector {
 public:
  // Constructs an uninitialized object; requires Init() to be called.
  EncodedS2PointVector() {}

  // Initializes the EncodedS2PointVector.
  //
  // REQUIRES: The Decoder data buffer must outlive this object.
  bool Init(Decoder* decoder);

  // Returns the size of the original vector.
  size_t size() const;

  // Returns the element at the given index.
  S2Point operator[](int i) const;

  // Decodes and returns the entire original vector.
  std::vector<S2Point> Decode() const;

 private:
  // We use a tagged union to represent multiple formats, as opposed to an
  // abstract base class or templating.  This represents the best compromise
  // between performance, space, and convenience.  Note that the overhead of
  // checking the tag is trivial and will typically be branch-predicted
  // perfectly.
  //
  // TODO(ericv): Once additional formats have been implemented, consider
  // using std::variant<> instead.  It's unclear whether this would have
  // better or worse performance than the current approach.
  enum Format : uint8 {
    UNCOMPRESSED = 0,
    CELL_ID = 1,
  };
  Format format_;
  uint32 size_;
  union {
    struct {
      const S2Point* points;
    } uncompressed_;
    struct {
      // TODO(ericv): Implement.
    } cell_id_;
  };
  friend void EncodeS2PointVector(absl::Span<const S2Point>, CodingHint,
                                  Encoder*);
};


//////////////////   Implementation details follow   ////////////////////


inline size_t EncodedS2PointVector::size() const {
  return size_;
}

inline S2Point EncodedS2PointVector::operator[](int i) const {
  switch (format_) {
    case UNCOMPRESSED:
      return uncompressed_.points[i];

    case CELL_ID:
      S2_LOG(FATAL) << "Not implemented yet";
      return S2Point();
  }
}

}  // namespace s2coding

#endif  // S2_ENCODED_S2POINT_VECTOR_H_
