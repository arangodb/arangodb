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

#include "s2/encoded_s2cell_id_vector.h"

using absl::Span;
using std::max;
using std::min;
using std::vector;

namespace s2coding {

void EncodeS2CellIdVector(Span<const S2CellId> v, Encoder* encoder) {
  // v[i] is encoded as (base + (deltas[i] << shift)).
  //
  // "base" consists of 0-7 bytes, and is always shifted so that its bytes are
  // the most-significant bytes of a uint64.
  //
  // "deltas" is an EncodedUintVector<uint64>, which means that all deltas
  // have a fixed-length encoding determined by the largest delta.
  //
  // "shift" is in the range 0..56.  The shift value is odd only if all
  // S2CellIds are at the same level, in which case the bit at position
  // (shift - 1) is automatically set to 1 in "base".
  //
  // "base" (3 bits) and "shift" (6 bits) are encoded in either one or two
  // bytes as follows:
  //   - if (shift <= 4 or shift is even), then 1 byte
  //   - otherwise 2 bytes
  // Note that (shift == 1) means that all S2CellIds are leaf cells, and
  // (shift == 2) means that all S2CellIds are at level 29.
  uint64 v_or = 0, v_and = ~0ULL, v_min = ~0ULL, v_max = 0;
  for (auto cellid : v) {
    v_or |= cellid.id();
    v_and &= cellid.id();
    v_min = min(v_min, cellid.id());
    v_max = max(v_max, cellid.id());
  }
  // These variables represent the values that will used during encoding.
  uint64 e_base = 0;        // Base value.
  int e_base_len = 0;       // Number of bytes to represent "base".
  int e_shift = 0;          // Delta shift.
  int e_max_delta_msb = 0;  // Bit position of the MSB of the largest delta.
  if (v_or > 0) {
    // We only allow even shifts, unless all values have the same low bit (in
    // which case the shift is odd and the preceding bit is implicitly on).
    // There is no point in allowing shifts > 56 since deltas are encoded in
    // at least 1 byte each.
    e_shift = min(56, Bits::FindLSBSetNonZero64(v_or) & ~1);
    if (v_and & (1ULL << e_shift)) ++e_shift;  // All S2CellIds same level.

    // "base" consists of the "base_len" most significant bytes of the minimum
    // S2CellId.  We consider all possible values of "base_len" (0..7) and
    // choose the one that minimizes the total encoding size.
    uint64 e_bytes = ~0ULL;  // Best encoding size so far.
    for (int len = 0; len < 8; ++len) {
      // "t_base" is the base value being tested (first "len" bytes of v_min).
      // "t_max_delta_msb" is the most-significant bit position of the largest
      // delta (or zero if there are no deltas, i.e. if v.size() == 0).
      // "t_bytes" is the total size of the variable portion of the encoding.
      uint64 t_base = v_min & ~(~0ULL >> (8 * len));
      int t_max_delta_msb =
          max(0, Bits::Log2Floor64((v_max - t_base) >> e_shift));
      uint64 t_bytes = len + v.size() * ((t_max_delta_msb >> 3) + 1);
      if (t_bytes < e_bytes) {
        e_base = t_base;
        e_base_len = len;
        e_max_delta_msb = t_max_delta_msb;
        e_bytes = t_bytes;
      }
    }
    // It takes one extra byte to encode odd shifts (i.e., the case where all
    // S2CellIds are at the same level), so check whether we can get the same
    // encoding size per delta using an even shift.
    if ((e_shift & 1) && (e_max_delta_msb & 7) != 7) --e_shift;
  }
  S2_DCHECK_LE(e_base_len, 7);
  S2_DCHECK_LE(e_shift, 56);
  encoder->Ensure(2 + e_base_len);

  // As described above, "shift" and "base_len" are encoded in 1 or 2 bytes.
  // "shift_code" is 5 bits:
  //   values <= 28 represent even shifts in the range 0..56
  //   values 29, 30 represent odd shifts 1 and 3
  //   value 31 indicates that the shift is odd and encoded in the next byte
  int shift_code = e_shift >> 1;
  if (e_shift & 1) shift_code = min(31, shift_code + 29);
  encoder->put8((shift_code << 3) | e_base_len);
  if (shift_code == 31) {
    encoder->put8(e_shift >> 1);  // Shift is always odd, so 3 bits unused.
  }
  // Encode the "base_len" most-significant bytes of "base".
  uint64 base_bytes = e_base >> (64 - 8 * max(1, e_base_len));
  EncodeUintWithLength<uint64>(base_bytes, e_base_len, encoder);

  // Finally, encode the vector of deltas.
  vector<uint64> deltas;
  deltas.reserve(v.size());
  for (auto cellid : v) {
    deltas.push_back((cellid.id() - e_base) >> e_shift);
  }
  EncodeUintVector<uint64>(deltas, encoder);
}

bool EncodedS2CellIdVector::Init(Decoder* decoder) {
  // All encodings have at least 2 bytes (one for our header and one for the
  // EncodedUintVector header), so this is safe.
  if (decoder->avail() < 2) return false;

  // Invert the encoding of (shift_code, base_len) described above.
  int code_plus_len = decoder->get8();
  int shift_code = code_plus_len >> 3;
  if (shift_code == 31) {
    shift_code = 29 + decoder->get8();
  }
  // Decode the "base_len" most-significant bytes of "base".
  int base_len = code_plus_len & 7;
  if (!DecodeUintWithLength(base_len, decoder, &base_)) return false;
  base_ <<= 64 - 8 * max(1, base_len);

  // Invert the encoding of "shift_code" described above.
  if (shift_code >= 29) {
    shift_ = 2 * (shift_code - 29) + 1;
    base_ |= 1ULL << (shift_ - 1);
  } else {
    shift_ = 2 * shift_code;
  }
  return deltas_.Init(decoder);
}

vector<S2CellId> EncodedS2CellIdVector::Decode() const {
  vector<S2CellId> result(size());
  for (int i = 0; i < size(); ++i) {
    result[i] = (*this)[i];
  }
  return result;
}

}  // namespace s2coding
