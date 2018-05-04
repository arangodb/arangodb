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

#include "s2/encoded_s2point_vector.h"

using absl::Span;
using std::vector;

namespace s2coding {

// To save space (especially for vectors of length 0, 1, and 2), the encoding
// format is encoded in the low-order 3 bits of the vector size.  Up to 7
// encoding formats are supported (only 2 are currently defined).  Additional
// formats could be supported by using "7" as an overflow indicator and
// encoding the actual format separately, but it seems unlikely we will ever
// need to do that.
static const int kEncodingFormatBits = 3;

void EncodeS2PointVector(Span<const S2Point> v, CodingHint hint,
                         Encoder* encoder) {
  // TODO(ericv): Implement CodingHint::COMPACT.

  encoder->Ensure(Varint::kMax64 + v.size() * sizeof(S2Point));
  uint64 size_format = (v.size() << kEncodingFormatBits |
                        EncodedS2PointVector::UNCOMPRESSED);
  encoder->put_varint64(size_format);
  encoder->putn(v.data(), v.size() * sizeof(S2Point));
}

bool EncodedS2PointVector::Init(Decoder* decoder) {
  uint64 size_format;
  if (!decoder->get_varint64(&size_format)) return false;
  format_ = static_cast<Format>(size_format & ((1 << kEncodingFormatBits) - 1));
  if (format_ != UNCOMPRESSED) return false;

  // Note that the encoding format supports up to 2**59 vertices, but we
  // currently only support decoding up to 2**32 vertices.
  size_format >>= kEncodingFormatBits;
  if (size_format > std::numeric_limits<uint32>::max()) return false;
  size_ = size_format;

  size_t bytes = size_t{size_} * sizeof(S2Point);
  if (decoder->avail() < bytes) return false;

  uncompressed_.points = reinterpret_cast<const S2Point*>(decoder->ptr());
  decoder->skip(bytes);
  return true;
}

vector<S2Point> EncodedS2PointVector::Decode() const {
  return vector<S2Point>(uncompressed_.points, uncompressed_.points + size_);
}

}  // namespace s2coding
