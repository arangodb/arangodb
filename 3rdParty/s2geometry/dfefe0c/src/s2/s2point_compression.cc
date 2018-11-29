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


#include "s2/s2point_compression.h"

#include <utility>
#include <vector>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/s2cell_id.h"
#include "s2/s2coords.h"
#include "s2/third_party/absl/base/casts.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/third_party/absl/container/fixed_array.h"
#include "s2/third_party/absl/types/span.h"
#include "s2/util/bits/bit-interleave.h"
#include "s2/util/coding/coder.h"
#include "s2/util/coding/nth-derivative.h"
#include "s2/util/coding/transforms.h"
#include "s2/util/endian/endian.h"

using absl::Span;
using std::pair;
using std::vector;

namespace {

const int kDerivativeEncodingOrder = 2;

// Pair of face number and count for run-length encoding.
struct FaceRun {
  FaceRun() : face(-1), count(0) {}
  FaceRun(int initial_face, int initial_count)
      : face(initial_face), count(initial_count) {}

  // Encodes each face as a varint64 with value kNumFaces * count + face.
  // 21 faces can fit in a single byte.  Varint64 is used so that 4G faces
  // can be encoded instead of just 4G / 6 = ~700M.
  void Encode(Encoder* encoder) const {
    encoder->Ensure(Encoder::kVarintMax64);

    // It isn't necessary to encode the number of faces left for the last run,
    // but since this would only help if there were more than 21 faces, it will
    // be a small overall savings, much smaller than the bound encoding.
    encoder->put_varint64(
        S2CellId::kNumFaces * absl::implicit_cast<int64>(count) + face);
    S2_DCHECK_GE(encoder->avail(), 0);
  }

  bool Decode(Decoder* decoder) {
    uint64 face_and_count;
    if (!decoder->get_varint64(&face_and_count)) return false;

    face = face_and_count % S2CellId::kNumFaces;
    // Make sure large counts don't wrap on malicious or random input.
    const uint64 count64 = face_and_count / S2CellId::kNumFaces;
    count = count64;

    return count > 0 && count == count64;
  }

  int face;
  int count;
};

// Run-length encoder/decoder for face numbers.
class Faces {
 public:
  class Iterator {
   public:
    explicit Iterator(const Faces& faces);

    // Return the next face.
    int Next();

   private:
    // The faces_ vector of the Faces object for which this is an iterator.
    const vector<FaceRun>& faces_;

    // The index that the next face will come from.
    int face_index_;

    // Number of faces already consumed for face_index_.
    int num_faces_used_for_index_;
  };

  Faces() {}

  // Add the face to the list of face runs, combining with the last if
  // possible.
  void AddFace(int face);

  // Encodes the faces to encoder.
  void Encode(Encoder* encoder) const;

  // Decodes the faces, returning true on success.
  bool Decode(int num_vertices, Decoder* decoder);

  Iterator GetIterator() const {
    Iterator iterator(*this);
    return iterator;
  }

 private:
  // Run-length encoded list of faces.
  vector<FaceRun> faces_;

  Faces(const Faces&) = delete;
  void operator=(const Faces&) = delete;
};

void Faces::AddFace(int face) {
  if (!faces_.empty() && faces_.back().face == face) {
    ++faces_.back().count;
  } else {
    faces_.push_back(FaceRun(face, 1));
  }
}

void Faces::Encode(Encoder* encoder) const {
  for (const FaceRun& face_run : faces_)
    face_run.Encode(encoder);
}

bool Faces::Decode(int num_vertices, Decoder* decoder) {
  for (int num_faces_parsed = 0; num_faces_parsed < num_vertices; ) {
    FaceRun face_run;
    if (!face_run.Decode(decoder)) return false;
    faces_.push_back(face_run);

    num_faces_parsed += face_run.count;
  }

  return true;
}

Faces::Iterator::Iterator(const Faces& faces)
    : faces_(faces.faces_), face_index_(0),
      num_faces_used_for_index_(0) {
}

int Faces::Iterator::Next() {
  S2_DCHECK_NE(faces_.size(), face_index_);
  S2_DCHECK_LE(num_faces_used_for_index_, faces_[face_index_].count);
  if (num_faces_used_for_index_ == faces_[face_index_].count) {
    ++face_index_;
    num_faces_used_for_index_ = 0;
  }

  ++num_faces_used_for_index_;
  return faces_[face_index_].face;
}

// Unused function (for documentation purposes only).
inline int STtoPiQi(double s, int level) {
  // We introduce a new coordinate system (pi, qi), which is (si, ti)
  // with the bits that are constant for cells of that level shifted
  // off to the right.
  // si = round(s * 2^31)
  // pi = si >> (31 - level)
  //    = floor(s * 2^level)
  // If the point has been snapped to the level, the bits that are
  // shifted off will be a 1 in the msb, then 0s after that, so the
  // fractional part discarded by the cast is (close to) 0.5.
  return static_cast<int>(s * (1 << level));
}

inline int SiTitoPiQi(unsigned int si, int level) {
  // See STtoPiQi for the definition of the PiQi coordinate system.
  //
  // EncodeFirstPointFixedLength encodes the return value using "level" bits,
  // so we clamp "si" to the range [0, 2**level - 1] before trying to encode
  // it.  This is okay because if si == kMaxSiTi, then it is not a cell center
  // anyway and will be encoded separately as an "off-center" point.
  si = std::min(si, S2::kMaxSiTi - 1);
  return si >> (S2::kMaxCellLevel + 1 - level);
}

inline double PiQitoST(int pi, int level) {
  // We want to recover the position at the center of the cell.  If the point
  // was snapped to the center of the cell, then modf(s * 2^level) == 0.5.
  // Inverting STtoPiQi gives:
  // s = (pi + 0.5) / 2^level.
  return (pi + 0.5) / (1 << level);
}

S2Point FacePiQitoXYZ(int face, int pi, int qi, int level) {
  return S2::FaceUVtoXYZ(face,
                         S2::STtoUV(PiQitoST(pi, level)),
                         S2::STtoUV(PiQitoST(qi, level))).Normalize();
}

void EncodeFirstPointFixedLength(const pair<int, int>& vertex_pi_qi,
                                 int level,
                                 NthDerivativeCoder* pi_coder,
                                 NthDerivativeCoder* qi_coder,
                                 Encoder* encoder) {
  // Do not ZigZagEncode the first point, since it cannot be negative.
  const uint32 pi = pi_coder->Encode(vertex_pi_qi.first);
  const uint32 qi = qi_coder->Encode(vertex_pi_qi.second);
  // Interleave to reduce overhead from two partial bytes to one.
  const uint64 interleaved_pi_qi = util_bits::InterleaveUint32(pi, qi);

  // Convert to little endian for architecture independence.
  const uint64 little_endian_interleaved_pi_qi =
      LittleEndian::FromHost64(interleaved_pi_qi);

  const int bytes_required = (level + 7) / 8 * 2;
  encoder->Ensure(bytes_required);
  encoder->putn(&little_endian_interleaved_pi_qi, bytes_required);
  S2_DCHECK_GE(encoder->avail(), 0);
}

void EncodePointCompressed(const pair<int, int>& vertex_pi_qi,
                           int level,
                           NthDerivativeCoder* pi_coder,
                           NthDerivativeCoder* qi_coder,
                           Encoder* encoder) {
  // ZigZagEncode, as varint requires the maximum number of bytes for
  // negative numbers.
  const uint32 zig_zag_encoded_deriv_pi =
      ZigZagEncode(pi_coder->Encode(vertex_pi_qi.first));
  const uint32 zig_zag_encoded_deriv_qi =
      ZigZagEncode(qi_coder->Encode(vertex_pi_qi.second));
  // Interleave to reduce overhead from two partial bytes to one.
  const uint64 interleaved_zig_zag_encoded_derivs =
      util_bits::InterleaveUint32(zig_zag_encoded_deriv_pi,
                                  zig_zag_encoded_deriv_qi);

  encoder->Ensure(Encoder::kVarintMax64);
  encoder->put_varint64(interleaved_zig_zag_encoded_derivs);
  S2_DCHECK_GE(encoder->avail(), 0);
}

void EncodePointsCompressed(Span<const pair<int, int>> vertices_pi_qi,
                            int level, Encoder* encoder) {
  NthDerivativeCoder pi_coder(kDerivativeEncodingOrder);
  NthDerivativeCoder qi_coder(kDerivativeEncodingOrder);
  for (int i = 0; i < vertices_pi_qi.size(); ++i) {
    if (i == 0) {
      // The first point will be just the (pi, qi) coordinates
      // of the S2Point.  NthDerivativeCoder will not save anything
      // in that case, so we encode in fixed format rather than varint
      // to avoid the varint overhead.
      EncodeFirstPointFixedLength(vertices_pi_qi[i], level,
                                  &pi_coder, &qi_coder, encoder);
    } else {
      EncodePointCompressed(vertices_pi_qi[i], level,
                            &pi_coder, &qi_coder, encoder);
    }
  }

  S2_DCHECK_GE(encoder->avail(), 0);
}

bool DecodeFirstPointFixedLength(Decoder* decoder,
                                 int level,
                                 NthDerivativeCoder* pi_coder,
                                 NthDerivativeCoder* qi_coder,
                                 pair<int, int>* vertex_pi_qi) {
  const int bytes_required = (level + 7) / 8 * 2;
  if (decoder->avail() < bytes_required) return false;
  uint64 little_endian_interleaved_pi_qi = 0;
  decoder->getn(&little_endian_interleaved_pi_qi, bytes_required);

  const uint64 interleaved_pi_qi =
      LittleEndian::ToHost64(little_endian_interleaved_pi_qi);

  uint32 pi, qi;
  util_bits::DeinterleaveUint32(interleaved_pi_qi, &pi, &qi);

  vertex_pi_qi->first = pi_coder->Decode(pi);
  vertex_pi_qi->second = qi_coder->Decode(qi);

  return true;
}

bool DecodePointCompressed(Decoder* decoder,
                           int level,
                           NthDerivativeCoder* pi_coder,
                           NthDerivativeCoder* qi_coder,
                           pair<int, int>* vertex_pi_qi) {
  uint64 interleaved_zig_zag_encoded_deriv_pi_qi;
  if (!decoder->get_varint64(&interleaved_zig_zag_encoded_deriv_pi_qi)) {
    return false;
  }

  uint32 zig_zag_encoded_deriv_pi, zig_zag_encoded_deriv_qi;
  util_bits::DeinterleaveUint32(interleaved_zig_zag_encoded_deriv_pi_qi,
                                &zig_zag_encoded_deriv_pi,
                                &zig_zag_encoded_deriv_qi);

  vertex_pi_qi->first =
      pi_coder->Decode(ZigZagDecode(zig_zag_encoded_deriv_pi));
  vertex_pi_qi->second =
      qi_coder->Decode(ZigZagDecode(zig_zag_encoded_deriv_qi));

  return true;
}

}  // namespace

void S2EncodePointsCompressed(Span<const S2XYZFaceSiTi> points,
                              int level,
                              Encoder* encoder) {
  absl::FixedArray<pair<int, int>> vertices_pi_qi(points.size());
  vector<int> off_center;
  Faces faces;
  for (int i = 0; i < points.size(); ++i) {
    faces.AddFace(points[i].face);
    vertices_pi_qi[i].first = SiTitoPiQi(points[i].si, level);
    vertices_pi_qi[i].second = SiTitoPiQi(points[i].ti, level);
    if (points[i].cell_level != level) {
      off_center.push_back(i);
    }
  }
  faces.Encode(encoder);
  EncodePointsCompressed(vertices_pi_qi, level, encoder);
  int num_off_center = off_center.size();
  encoder->Ensure(Encoder::kVarintMax32 +
                  (Encoder::kVarintMax32 + sizeof(S2Point)) * num_off_center);
  encoder->put_varint32(num_off_center);
  S2_DCHECK_GE(encoder->avail(), 0);
  for (int index : off_center) {
    encoder->put_varint32(index);
    encoder->putn(&points[index].xyz, sizeof(points[index].xyz));
    S2_DCHECK_GE(encoder->avail(), 0);
  }
}

bool S2DecodePointsCompressed(Decoder* decoder, int level,
                              Span<S2Point> points) {
  Faces faces;
  if (!faces.Decode(points.size(), decoder)) {
    return false;
  }

  NthDerivativeCoder pi_coder(kDerivativeEncodingOrder);
  NthDerivativeCoder qi_coder(kDerivativeEncodingOrder);
  Faces::Iterator faces_iterator = faces.GetIterator();
  for (int i = 0; i < points.size(); ++i) {
    pair<int, int> vertex_pi_qi;
    if (i == 0) {
      if (!DecodeFirstPointFixedLength(decoder, level, &pi_coder, &qi_coder,
                                       &vertex_pi_qi)) {
        return false;
      }
    } else {
      if (!DecodePointCompressed(decoder, level, &pi_coder, &qi_coder,
                                 &vertex_pi_qi)) {
        return false;
      }
    }

    int face = faces_iterator.Next();
    points[i] =
        FacePiQitoXYZ(face, vertex_pi_qi.first, vertex_pi_qi.second, level);
  }

  unsigned int num_off_center;
  if (!decoder->get_varint32(&num_off_center) ||
      num_off_center > points.size()) {
    return false;
  }
  for (int i = 0; i < num_off_center; ++i) {
    uint32 index;
    if (!decoder->get_varint32(&index) || index >= points.size()) {
      return false;
    }
    if (decoder->avail() < sizeof(points[index])) return false;
    decoder->getn(&points[index], sizeof(points[index]));
  }
  return true;
}
