// Copyright 2011 Google Inc. All Rights Reserved.
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

//
// Given a sequence of S2Points assumed to be the center of level-k cells,
// compresses it into a stream using the following method:
// - decompose the points into (face, si, ti) tuples (see s2coords.h)
// - run-length encode the faces, combining face number and count into a
//     varint32.  See the Faces class in s2point_compression.cc.
// - right shift the (si, ti) to remove the part that's constant for all cells
//     of level-k.  The result is called the (pi, qi) space.
// - 2nd derivative encode the pi and qi sequences (linear prediction)
// - zig-zag encode all derivative values but the first, which cannot be
//     negative
// - interleave the zig-zag encoded values
// - encode the first interleaved value in a fixed length encoding
//     (varint would make this value larger)
// - encode the remaining interleaved values as varint64s, as the
//     derivative encoding should make the values small.
// In addition, provides a lossless method to compress a sequence of points even
// if some points are not the center of level-k cells. These points are stored
// exactly, using 3 double precision values, after the above encoded string,
// together with their index in the sequence (this leads to some redundancy - it
// is expected that only a small fraction of the points are not cell centers).
//
// Require that the encoder was constructed with the no-arg constructor, as
// Ensure() will be called to allocate space.

//
// To encode leaf cells, this requires 8 bytes for the first vertex plus
// an average of 3.8 bytes for each additional vertex, when computed on
// Google's geographic repository.

#ifndef S2_S2POINT_COMPRESSION_H_
#define S2_S2POINT_COMPRESSION_H_

#include "s2/third_party/absl/types/span.h"
#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"

class Decoder;
class Encoder;

// The XYZ and face,si,ti coordinates of an S2Point and, if this point is equal
// to the center of an S2Cell, the level of this cell (-1 otherwise).
struct S2XYZFaceSiTi {
  S2Point xyz;
  int face;
  unsigned int si;
  unsigned int ti;
  int cell_level;
};

// Encode the points in the encoder, using an optimized compressed format for
// points at the center of a cell at 'level', plus 3 double values for the
// others.
void S2EncodePointsCompressed(absl::Span<const S2XYZFaceSiTi> points,
                              int level, Encoder* encoder);

// Decode points encoded with S2EncodePointsCompressed. Requires that the
// level is the level that was used in S2EncodePointsCompressed. Ensures
// that the decoded points equal the encoded points. Returns true on success.
bool S2DecodePointsCompressed(Decoder* decoder, int level,
                              absl::Span<S2Point> points);

#endif  // S2_S2POINT_COMPRESSION_H_
