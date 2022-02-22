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

#include <algorithm>

#include "absl/base/internal/unaligned_access.h"
#include "s2/util/bits/bits.h"
#include "s2/s2cell_id.h"
#include "s2/s2coords.h"

using absl::MakeSpan;
using absl::Span;
using std::max;
using std::min;
using std::vector;

namespace s2coding {

// Like util_bits::InterleaveUint32, but interleaves bit pairs rather than
// individual bits.  This format is faster to decode than the fully interleaved
// format, and produces the same results for our use case.
inline uint64 InterleaveUint32BitPairs(const uint32 val0, const uint32 val1) {
  uint64 v0 = val0, v1 = val1;
  v0 = (v0 | (v0 << 16)) & 0x0000ffff0000ffff;
  v1 = (v1 | (v1 << 16)) & 0x0000ffff0000ffff;
  v0 = (v0 | (v0 << 8)) & 0x00ff00ff00ff00ff;
  v1 = (v1 | (v1 << 8)) & 0x00ff00ff00ff00ff;
  v0 = (v0 | (v0 << 4)) & 0x0f0f0f0f0f0f0f0f;
  v1 = (v1 | (v1 << 4)) & 0x0f0f0f0f0f0f0f0f;
  v0 = (v0 | (v0 << 2)) & 0x3333333333333333;
  v1 = (v1 | (v1 << 2)) & 0x3333333333333333;
  return v0 | (v1 << 2);
}

// This code is about 50% faster than util_bits::DeinterleaveUint32, which
// uses a lookup table.  The speed advantage is expected to be even larger in
// code that mixes bit interleaving with other significant operations since it
// doesn't require keeping a 256-byte lookup table in the L1 data cache.
inline void DeinterleaveUint32BitPairs(uint64 code,
                                       uint32 *val0, uint32 *val1) {
  uint64 v0 = code, v1 = code >> 2;
  v0 &= 0x3333333333333333;
  v0 |= v0 >> 2;
  v1 &= 0x3333333333333333;
  v1 |= v1 >> 2;
  v0 &= 0x0f0f0f0f0f0f0f0f;
  v0 |= v0 >> 4;
  v1 &= 0x0f0f0f0f0f0f0f0f;
  v1 |= v1 >> 4;
  v0 &= 0x00ff00ff00ff00ff;
  v0 |= v0 >> 8;
  v1 &= 0x00ff00ff00ff00ff;
  v1 |= v1 >> 8;
  v0 &= 0x0000ffff0000ffff;
  v0 |= v0 >> 16;
  v1 &= 0x0000ffff0000ffff;
  v1 |= v1 >> 16;
  *val0 = v0;
  *val1 = v1;
}

// Forward declarations.
void EncodeS2PointVectorFast(Span<const S2Point> points, Encoder* encoder);
void EncodeS2PointVectorCompact(Span<const S2Point> points, Encoder* encoder);

// To save space (especially for vectors of length 0, 1, and 2), the encoding
// format is encoded in the low-order 3 bits of the vector size.  Up to 7
// encoding formats are supported (only 2 are currently defined).  Additional
// formats could be supported by using "7" as an overflow indicator and
// encoding the actual format separately, but it seems unlikely we will ever
// need to do that.
static const int kEncodingFormatBits = 3;
static const uint8 kEncodingFormatMask = (1 << kEncodingFormatBits) - 1;

void EncodeS2PointVector(Span<const S2Point> points, CodingHint hint,
                         Encoder* encoder) {
  switch (hint) {
    case CodingHint::FAST:
      return EncodeS2PointVectorFast(points, encoder);

    case CodingHint::COMPACT:
      return EncodeS2PointVectorCompact(points, encoder);

    default:
      S2_LOG(DFATAL) << "Unknown CodingHint: " << static_cast<int>(hint);
  }
}

bool EncodedS2PointVector::Init(Decoder* decoder) {
  if (decoder->avail() < 1) return false;

  // Peek at the format but don't advance the decoder; the format-specific
  // Init functions will do that.
  format_ = static_cast<Format>(*decoder->skip(0) & kEncodingFormatMask);
  switch (format_) {
    case UNCOMPRESSED:
      return InitUncompressedFormat(decoder);

    case CELL_IDS:
      return InitCellIdsFormat(decoder);

    default:
      return false;
  }
}

vector<S2Point> EncodedS2PointVector::Decode() const {
  vector<S2Point> points;
  points.reserve(size_);
  for (int i = 0; i < size_; ++i) {
    points.push_back((*this)[i]);
  }
  return points;
}

// The encoding must be identical to EncodeS2PointVector().
void EncodedS2PointVector::Encode(Encoder* encoder) const {
  switch (format_) {
    case UNCOMPRESSED:
      EncodeS2PointVectorFast(MakeSpan(uncompressed_.points, size_), encoder);
      break;

    case CELL_IDS: {
      // This is a full decode/encode dance, and not at all efficient.
      EncodeS2PointVectorCompact(Decode(), encoder);
      break;
    }

    default:
      S2_LOG(FATAL) << "Unknown Format: " << static_cast<int>(format_);
  }
}

//////////////////////////////////////////////////////////////////////////////
//                     UNCOMPRESSED Encoding Format
//////////////////////////////////////////////////////////////////////////////

// Encodes a vector of points, optimizing for (encoding and decoding) speed.
void EncodeS2PointVectorFast(Span<const S2Point> points, Encoder* encoder) {
#ifndef IS_LITTLE_ENDIAN
  S2_LOG(FATAL) << "Not implemented on big-endian architectures";
#endif

  // This function always uses the UNCOMPRESSED encoding.  The header consists
  // of a varint64 in the following format:
  //
  //   bits 0-2:  encoding format (UNCOMPRESSED)
  //   bits 3-63: vector size
  //
  // This is followed by an array of S2Points in little-endian order.
  encoder->Ensure(Varint::kMax64 + points.size() * sizeof(S2Point));
  uint64 size_format = (points.size() << kEncodingFormatBits |
                        EncodedS2PointVector::UNCOMPRESSED);
  encoder->put_varint64(size_format);
  encoder->putn(points.data(), points.size() * sizeof(S2Point));
}

bool EncodedS2PointVector::InitUncompressedFormat(Decoder* decoder) {
#if !defined(IS_LITTLE_ENDIAN) || defined(__arm__) || \
  defined(ABSL_INTERNAL_NEED_ALIGNED_LOADS)
  // TODO(ericv): Make this work on platforms that don't support unaligned
  // 64-bit little-endian reads, e.g. by falling back to
  //
  //   bit_cast<double>(little_endian::Load64()).
  //
  // Maybe the compiler is smart enough that we can do this all the time,
  // but more likely we will need two cases using the #ifdef above.
  // (Note that even ARMv7 does not support unaligned 64-bit loads.)
  S2_LOG(DFATAL) << "Needs architecture with 64-bit little-endian unaligned loads";
  return false;
#endif

  uint64 size;
  if (!decoder->get_varint64(&size)) return false;
  size >>= kEncodingFormatBits;

  // Note that the encoding format supports up to 2**59 vertices, but we
  // currently only support decoding up to 2**32 vertices.
  if (size > std::numeric_limits<uint32>::max()) return false;
  size_ = size;

  size_t bytes = size_t{size_} * sizeof(S2Point);
  if (decoder->avail() < bytes) return false;

  uncompressed_.points = reinterpret_cast<const S2Point*>(decoder->skip(0));
  decoder->skip(bytes);
  return true;
}


//////////////////////////////////////////////////////////////////////////////
//                     CELL_IDS Encoding Format
//////////////////////////////////////////////////////////////////////////////

// Represents a point that can be encoded as an S2CellId center.
// (If such an encoding is not possible then level < 0.)
struct CellPoint {
  // Constructor necessary in order to narrow "int" arguments to "int8".
  CellPoint(int level, int face, uint32 si, uint32 ti)
    : level(level), face(face), si(si), ti(ti) {}

  int8 level, face;
  uint32 si, ti;
};

// S2CellIds are represented in a special 64-bit format and are encoded in
// fixed-size blocks.  kBlockSize represents the number of values per block.
// Block sizes of 4, 8, 16, and 32 were tested and kBlockSize == 16 seems to
// offer the best compression.  (Note that kBlockSize == 32 requires some code
// modifications which have since been removed.)
static constexpr int kBlockShift = 4;
static constexpr size_t kBlockSize = 1 << kBlockShift;

// Used to indicate that a point must be encoded as an exception (a 24-byte
// S2Point) rather than as an S2CellId.
static constexpr uint64 kException = ~0ULL;

// Represents the encoding parameters to be used for a given block (consisting
// of kBlockSize encodable 64-bit values).  See below.
struct BlockCode {
  int delta_bits;     // Delta length in bits (multiple of 4)
  int offset_bits;    // Offset length in bits (multiple of 8)
  int overlap_bits;   // {Delta, Offset} overlap in bits (0 or 4)
};

// Returns a bit mask with "n" low-order 1 bits, for 0 <= n <= 64.
inline uint64 BitMask(int n) {
  return (n == 0) ? 0 : (~0ULL >> (64 - n));
}

// Returns the maximum number of bits per value at the given S2CellId level.
inline int MaxBitsForLevel(int level) {
  return 2 * level + 3;
}

// Returns the number of bits that "base" should be right-shifted in order to
// encode only its leading "base_bits" bits, assuming that all points are
// encoded at the given S2CellId level.
inline int BaseShift(int level, int base_bits) {
  return max(0, MaxBitsForLevel(level) - base_bits);
}

// Forward declarations.
int ChooseBestLevel(Span<const S2Point> points, vector<CellPoint>* cell_points);
vector<uint64> ConvertCellsToValues(const vector<CellPoint>& cell_points,
                                    int level, bool* have_exceptions);
uint64 ChooseBase(const vector<uint64>& values, int level, bool have_exceptions,
                  int* base_bits);
BlockCode GetBlockCode(Span<const uint64> values, uint64 base,
                       bool have_exceptions);

// Encodes a vector of points, optimizing for space.
void EncodeS2PointVectorCompact(Span<const S2Point> points, Encoder* encoder) {
  // OVERVIEW
  // --------
  //
  // We attempt to represent each S2Point as the center of an S2CellId.  All
  // S2CellIds must be at the same level.  Any points that cannot be encoded
  // exactly as S2CellId centers are stored as exceptions using 24 bytes each.
  // If there are so many exceptions that the CELL_IDS encoding does not save
  // significant space, we give up and use the uncompressed encoding.
  //
  // The first step is to choose the best S2CellId level.  This requires
  // converting each point to (face, si, ti) coordinates and checking whether
  // the point can be represented exactly as an S2CellId center at some level.
  // We then build a histogram of S2CellId levels (just like the similar code
  // in S2Polygon::Encode) and choose the best level (or give up, if there are
  // not enough S2CellId-encodable points).
  //
  // The simplest approach would then be to take all the S2CellIds and
  // right-shift them to remove all the constant bits at the chosen level.
  // This would give the best spatial locality and hence the smallest deltas.
  // However instead we give up some spatial locality and use the similar but
  // faster transformation described below.
  //
  // Each encodable point is first converted to the (sj, tj) representation
  // defined below:
  //
  //   sj = (((face & 3) << 30) | (si >> 1)) >> (30 - level);
  //   tj = (((face & 4) << 29) | ti) >> (31 - level);
  //
  // These two values encode the (face, si, ti) tuple using (2 * level + 3)
  // bits.  To see this, recall that "si" and "ti" are 31-bit values that all
  // share a common suffix consisting of a "1" bit followed by (30 - level)
  // "0" bits.  The code above right-shifts these values to remove the
  // constant bits and then prepends the bits for "face", yielding a total of
  // (level + 2) bits for "sj" and (level + 1) bits for "tj".
  //
  // We then combine (sj, tj) into one 64-bit value by interleaving bit pairs:
  //
  //   v = InterleaveBitPairs(sj, tj);
  //
  // (We could also interleave individual bits, but it is faster this way.)
  // The result is similar to right-shifting an S2CellId by (61 - 2 * level),
  // except that it is faster to decode and the spatial locality is not quite
  // as good.
  //
  // The 64-bit values are divided into blocks of size kBlockSize, and then
  // each value is encoded as the sum of a base value, a per-block offset, and
  // a per-value delta within that block:
  //
  //   v[i,j] = base + offset[i] + delta[i, j]
  //
  // where "i" represents a block and "j" represents an entry in that block.
  //
  // The deltas for each block are encoded using a fixed number of 4-bit nibbles
  // (1-16 nibbles per delta).  This allows any delta to be accessed in constant
  // time.
  //
  // The "offset" for each block is a 64-bit value encoded in 0-8 bytes.  The
  // offset is left-shifted such that it overlaps the deltas by a configurable
  // number of bits (either 0 or 4), called the "overlap".  The overlap and
  // offset length (0-8 bytes) are specified per block.  The reason for the
  // overlap is that it allows fewer delta bits to be used in some cases.  For
  // example if base == 0 and the range within a block is 0xf0 to 0x110, then
  // rather than using 12-bits deltas with an offset of 0, the overlap lets us
  // use 8-bits deltas with an offset of 0xf0 (saving 7 bytes per block).
  //
  // The global minimum value "base" is encoded using 0-7 bytes starting with
  // the most-significant non-zero bit possible for the chosen level.  For
  // example, if (level == 7) then the encoded values have at most 17 bits, so
  // if "base" is encoded in 1 byte then it is shifted to occupy bits 9-16.
  //
  // Example: at level == 15, there are at most 33 non-zero value bits.  The
  // following shows the bit positions covered by "base", "offset", and "delta"
  // assuming that "base" and "offset" are encoded in 2 bytes each, deltas are
  // encoded in 2 nibbles (1 byte) each, and "overlap" is 4 bits:
  //
  //   Base:             1111111100000000-----------------
  //   Offset:           -------------1111111100000000----
  //   Delta:            -------------------------00000000
  //   Overlap:                                   ^^^^
  //
  // The numbers (0 or 1) in this diagram denote the byte number of the encoded
  // value.  Notice that "base" is shifted so that it starts at the leftmost
  // possible bit, "delta" always starts at the rightmost possible bit (bit 0),
  // and "offset" is shifted so that it overlaps "delta" by the chosen "overlap"
  // (either 0 or 4 bits).  Also note that all of these values are summed, and
  // therefore each value can affect higher-order bits due to carries.
  //
  // NOTE(ericv): Encoding deltas in 4-bit rather than 8-bit length increments
  // reduces encoded sizes by about 7%.  Allowing a 4-bit overlap between the
  // offset and deltas reduces encoded sizes by about 1%.  Both optimizations
  // make the code more complex but don't affect running times significantly.
  //
  // ENCODING DETAILS
  // ----------------
  //
  // Now we can move on to the actual encodings.  First, there is a 2 byte
  // header encoded as follows:
  //
  //  Byte 0, bits 0-2: encoding_format (CELL_IDS)
  //  Byte 0, bit  3:   have_exceptions
  //  Byte 0, bits 4-7: (last_block_size - 1)
  //  Byte 1, bits 0-2: base_bytes
  //  Byte 1, bits 3-7: level (0-30)
  //
  // This is followed by an EncodedStringVector containing the encoded blocks.
  // Each block contains kBlockSize (8) values.  The total size of the
  // EncodeS2PointVector is not stored explicity, but instead is calculated as
  //
  //     num_values == kBlockSize * (num_blocks - 1) + last_block_size .
  //
  // (An empty vector has num_blocks == 0 and last_block_size == kBlockSize.)
  //
  // Each block starts with a 1 byte header containing the following:
  //
  //  Byte 0, bits 0-2: (offset_bytes - overlap_nibbles)
  //  Byte 0, bit  3:   overlap_nibbles
  //  Byte 0, bits 4-7: (delta_nibbles - 1)
  //
  // "overlap_nibbles" is either 0 or 1 (indicating an overlap of 0 or 4 bits),
  // while "offset_bytes" is in the range 0-8 (indicating the number of bytes
  // used to encode the offset for this block).  Note that some combinations
  // cannot be encoded: in particular, offset_bytes == 0 can only be encoded
  // with an overlap of 0 bits, and offset_bytes == 8 can only be encoded with
  // an overlap of 4 bits.  This allows us to encode offset lengths of 0-8
  // rather than just 0-7 without using an extra bit.  (Note that the
  // combinations that can't be encoded are not useful anyway.)
  //
  // The header is followed by "offset_bytes" bytes for the offset, and then
  // (4 * delta_nibbles) bytes for the deltas.
  //
  // If there are any points that could not be represented as S2CellIds, then
  // "have_exceptions" in the header is true.  In that case the delta values
  // within each block are encoded as (delta + kBlockSize), and values
  // 0...kBlockSize-1 are used to represent exceptions.  If a block has
  // exceptions, they are encoded immediately following the array of deltas,
  // and are referenced by encoding the corresponding exception index
  // 0...kBlockSize-1 as the delta.
  //
  // TODO(ericv): A vector containing a single leaf cell is currently encoded as
  // 13 bytes (2 byte header, 7 byte base, 1 byte block count, 1 byte block
  // length, 1 byte block header, 1 byte delta).  However if this case occurs
  // often, a better solution would be implement a separate format that encodes
  // the leading k bytes of an S2CellId.  It would have a one-byte header
  // consisting of the encoding format (3 bits) and the number of bytes encoded
  // (3 bits), followed by the S2CellId bytes.  The extra 2 header bits could be
  // used to store single points using other encodings, e.g. E7.
  //
  // If we had used 8-value blocks, we could have used the extra bit in the
  // first byte of the header to indicate that there is only one value, and
  // then skip the 2nd byte of header and the EncodedStringVector.  But this
  // would be messy because it also requires special cases while decoding.
  // Essentially this would be a sub-format within the CELL_IDS format.

  // 1. Compute (level, face, si, ti) for each point, build a histogram of
  // levels, and determine the optimal level to use for encoding (if any).
  vector<CellPoint> cell_points;
  int level = ChooseBestLevel(points, &cell_points);
  if (level < 0) {
    return EncodeS2PointVectorFast(points, encoder);
  }

  // 2. Convert the points into encodable 64-bit values.  We don't use the
  // S2CellId itself because it requires a somewhat more complicated bit
  // interleaving operation.
  //
  // TODO(ericv): Benchmark using shifted S2CellIds instead.
  bool have_exceptions;
  vector<uint64> values = ConvertCellsToValues(cell_points, level,
                                               &have_exceptions);

  // 3. Choose the global encoding parameter "base" (consisting of the bit
  // prefix shared by all values to be encoded).
  int base_bits;
  uint64 base = ChooseBase(values, level, have_exceptions, &base_bits);

  // Now encode the output, starting with the 2-byte header (see above).
  int num_blocks = (values.size() + kBlockSize - 1) >> kBlockShift;
  int base_bytes = base_bits >> 3;
  encoder->Ensure(2 + base_bytes);
  int last_block_count = values.size() - kBlockSize * (num_blocks - 1);
  S2_DCHECK_GE(last_block_count, 0);
  S2_DCHECK_LE(last_block_count, kBlockSize);
  S2_DCHECK_LE(base_bytes, 7);
  S2_DCHECK_LE(level, 30);
  encoder->put8(EncodedS2PointVector::CELL_IDS |
                (have_exceptions << 3) |
                ((last_block_count - 1) << 4));
  encoder->put8(base_bytes | (level << 3));

  // Next we encode 0-7 bytes of "base".
  int base_shift = BaseShift(level, base_bits);
  EncodeUintWithLength(base >> base_shift, base_bytes, encoder);

  // Now we encode the contents of each block.
  StringVectorEncoder blocks;
  vector<S2Point> exceptions;
  uint64 offset_bytes_sum = 0;
  uint64 delta_nibbles_sum = 0;
  uint64 exceptions_sum = 0;
  for (int i = 0; i < values.size(); i += kBlockSize) {
    int block_size = min(kBlockSize, values.size() - i);
    BlockCode code = GetBlockCode(MakeSpan(&values[i], block_size),
                                  base, have_exceptions);

    // Encode the one-byte block header (see above).
    Encoder* block = blocks.AddViaEncoder();
    int offset_bytes = code.offset_bits >> 3;
    int delta_nibbles = code.delta_bits >> 2;
    int overlap_nibbles = code.overlap_bits >> 2;
    block->Ensure(1 + offset_bytes + (kBlockSize / 2) * delta_nibbles);
    S2_DCHECK_LE(offset_bytes - overlap_nibbles, 7);
    S2_DCHECK_LE(overlap_nibbles, 1);
    S2_DCHECK_LE(delta_nibbles, 16);
    block->put8((offset_bytes - overlap_nibbles) |
                (overlap_nibbles << 3) | (delta_nibbles - 1) << 4);

    // Determine the offset for this block, and whether there are exceptions.
    uint64 offset = ~0ULL;
    int num_exceptions = 0;
    for (int j = 0; j < block_size; ++j) {
      if (values[i + j] == kException) {
        num_exceptions += 1;
      } else {
        S2_DCHECK_GE(values[i + j], base);
        offset = min(offset, values[i + j] - base);
      }
    }
    if (num_exceptions == block_size) offset = 0;

    // Encode the offset.
    int offset_shift = code.delta_bits - code.overlap_bits;
    offset &= ~BitMask(offset_shift);
    S2_DCHECK_EQ(offset == 0, offset_bytes == 0);
    if (offset > 0) {
      EncodeUintWithLength(offset >> offset_shift, offset_bytes, block);
    }

    // Encode the deltas, and also gather any exceptions present.
    int delta_bytes = (delta_nibbles + 1) >> 1;
    exceptions.clear();
    for (int j = 0; j < block_size; ++j) {
      uint64 delta;
      if (values[i + j] == kException) {
        delta = exceptions.size();
        exceptions.push_back(points[i + j]);
      } else {
        S2_DCHECK_GE(values[i + j], offset + base);
        delta = values[i + j] - (offset + base);
        if (have_exceptions) {
          S2_DCHECK_LE(delta, ~0ULL - kBlockSize);
          delta += kBlockSize;
        }
      }
      S2_DCHECK_LE(delta, BitMask(code.delta_bits));
      if ((delta_nibbles & 1) && (j & 1)) {
        // Combine this delta with the high-order 4 bits of the previous delta.
        uint8 last_byte = *(block->base() + block->length() - 1);
        block->RemoveLast(1);
        delta = (delta << 4) | (last_byte & 0xf);
      }
      EncodeUintWithLength(delta, delta_bytes, block);
    }
    // Append any exceptions to the end of the block.
    if (num_exceptions > 0) {
      int exceptions_bytes = exceptions.size() * sizeof(S2Point);
      block->Ensure(exceptions_bytes);
      block->putn(exceptions.data(), exceptions_bytes);
    }
    offset_bytes_sum += offset_bytes;
    delta_nibbles_sum += delta_nibbles;
    exceptions_sum += num_exceptions;
  }
  blocks.Encode(encoder);
}

// Returns the S2CellId level for which the greatest number of the given points
// can be represented as the center of an S2CellId.  Initializes "cell_points"
// to contain the S2CellId representation of each point (if any).  Returns -1
// if there is no S2CellId that would result in significant space savings.
int ChooseBestLevel(Span<const S2Point> points,
                    vector<CellPoint>* cell_points) {
  cell_points->clear();
  cell_points->reserve(points.size());

  // Count the number of points at each level.
  int level_counts[S2CellId::kMaxLevel + 1] = { 0 };
  for (const S2Point& point : points) {
    int face;
    uint32 si, ti;
    int level = S2::XYZtoFaceSiTi(point, &face, &si, &ti);
    cell_points->push_back(CellPoint(level, face, si, ti));
    if (level >= 0) ++level_counts[level];
  }
  // Choose the level for which the most points can be encoded.
  int best_level = 0;
  for (int level = 1; level <= S2CellId::kMaxLevel; ++level) {
    if (level_counts[level] > level_counts[best_level]) {
      best_level = level;
    }
  }
  // The uncompressed encoding is smaller *and* faster when very few of the
  // points are encodable as S2CellIds.  The CELL_IDS encoding uses about 1
  // extra byte per point in this case, consisting of up to a 3 byte
  // EncodedStringVector offset for each block, a 1 byte block header, and 4
  // bits per delta (encoding an exception number from 0-7), for a total of 8
  // bytes per block.  This represents a space overhead of about 4%, so we
  // require that at least 5% of the input points should be encodable as
  // S2CellIds in order for the CELL_IDS format to be worthwhile.
  constexpr double kMinEncodableFraction = 0.05;
  if (level_counts[best_level] <= kMinEncodableFraction * points.size()) {
    return -1;
  }
  return best_level;
}

// Given a vector of points in CellPoint format and an S2CellId level that has
// been chosen for encoding, returns a vector of 64-bit values that should be
// encoded in order to represent these points.  Points that cannot be
// represented losslessly as the center of an S2CellId at the chosen level are
// indicated by the value "kException".  "have_exceptions" is set to indicate
// whether any exceptions were present.
vector<uint64> ConvertCellsToValues(const vector<CellPoint>& cell_points,
                                    int level, bool* have_exceptions) {
  vector<uint64> values;
  values.reserve(cell_points.size());
  *have_exceptions = false;
  int shift = S2CellId::kMaxLevel - level;
  for (CellPoint cp : cell_points) {
    if (cp.level != level) {
      values.push_back(kException);
      *have_exceptions = true;
    } else {
      // Note that bit 31 of tj is always zero, and that bits are interleaved in
      // such a way that bit 63 of the result is always zero.
      //
      // The S2CellId version of the following code is:
      // uint64 v = S2CellId::FromFaceIJ(cp.face, cp.si >> 1, cp.ti >> 1).
      //            parent(level).id() >> (2 * shift + 1);
      uint32 sj = (((cp.face & 3) << 30) | (cp.si >> 1)) >> shift;
      uint32 tj = (((cp.face & 4) << 29) | cp.ti) >> (shift + 1);
      uint64 v = InterleaveUint32BitPairs(sj, tj);
      S2_DCHECK_LE(v, BitMask(MaxBitsForLevel(level)));
      values.push_back(v);
    }
  }
  return values;
}

uint64 ChooseBase(const vector<uint64>& values, int level, bool have_exceptions,
                  int* base_bits) {
  // Find the minimum and maximum non-exception values to be represented.
  uint64 v_min = kException, v_max = 0;
  for (auto v : values) {
    if (v != kException) {
      v_min = min(v_min, v);
      v_max = max(v_max, v);
    }
  }
  if (v_min == kException) return 0;

  // Generally "base" is chosen as the bit prefix shared by v_min and v_max.
  // However there are a few adjustments we need to make.
  //
  // 1. Encodings are usually smaller if the bits represented by "base" and
  // "delta" do not overlap.  Usually the shared prefix rule does this
  // automatically, but if v_min == v_max or there are special circumstances
  // that increase delta_bits (such as values.size() == 1) then we need to
  // make an adjustment.
  //
  // 2. The format only allows us to represent up to 7 bytes (56 bits) of
  // "base", so we need to ensure that "base" conforms to this requirement.
  int min_delta_bits = (have_exceptions || values.size() == 1) ? 8 : 4;
  int excluded_bits = max(Bits::Log2Floor64(v_min ^ v_max) + 1,
                          max(min_delta_bits, BaseShift(level, 56)));
  uint64 base = v_min & ~BitMask(excluded_bits);

  // Determine how many bytes are needed to represent this prefix.
  if (base == 0) {
    *base_bits = 0;
  } else {
    int low_bit = Bits::FindLSBSetNonZero64(base);
    *base_bits = (MaxBitsForLevel(level) - low_bit + 7) & ~7;
  }

  // Since base_bits has been rounded up to a multiple of 8, we may now be
  // able to represent additional bits of v_min.  In general this reduces the
  // final encoded size.
  //
  // NOTE(ericv): A different strategy for choosing "base" is to encode all
  // blocks under the assumption that "base" equals v_min exactly, and then
  // set base equal to the minimum-length prefix of "v_min" that allows these
  // encodings to be used.  This strategy reduces the encoded sizes by
  // about 0.2% relative to the strategy here, but is more complicated.
  return v_min & ~BitMask(BaseShift(level, *base_bits));
}

// Returns true if the range of values [d_min, d_max] can be encoded using the
// specified parameters (delta_bits, overlap_bits, and have_exceptions).
bool CanEncode(uint64 d_min, uint64 d_max, int delta_bits,
               int overlap_bits, bool have_exceptions) {
  // "offset" can't represent the lowest (delta_bits - overlap_bits) of d_min.
  d_min &= ~BitMask(delta_bits - overlap_bits);

  // The maximum delta is reduced by kBlockSize if any exceptions exist, since
  // deltas 0..kBlockSize-1 are used to indicate exceptions.
  uint64 max_delta = BitMask(delta_bits);
  if (have_exceptions) {
    if (max_delta < kBlockSize) return false;
    max_delta -= kBlockSize;
  }
  // The first test below is necessary to avoid 64-bit overflow.
  return (d_min > ~max_delta) || (d_min + max_delta >= d_max);
}

// Given a vector of 64-bit values to be encoded and an S2CellId level, returns
// the optimal encoding parameters that should be used to encode each block.
// Also returns the global minimum value "base" and the number of bits that
// should be used to encode it ("base_bits").
BlockCode GetBlockCode(Span<const uint64> values, uint64 base,
                       bool have_exceptions) {
  // "b_min" and "b_max"n are the minimum and maximum values within this block.
  uint64 b_min = kException, b_max = 0;
  for (uint64 v : values) {
    if (v != kException) {
      b_min = min(b_min, v);
      b_max = max(b_max, v);
    }
  }
  if (b_min == kException) {
    // All values in this block are exceptions.
    return BlockCode{4, 0, 0};
  }

  // Adjust the min/max values so that they are relative to "base".
  b_min -= base;
  b_max -= base;

  // Determine the minimum possible delta length and overlap that can be used
  // to encode this block.  The block will usually be encodable using the
  // number of bits in (b_max - b_min) rounded up to a multiple of 4.  If this
  // is not possible, the preferred solution is to shift "offset" so that the
  // delta and offset values overlap by 4 bits (since this only costs an
  // average of 4 extra bits per block).  Otherwise we increase the delta size
  // by 4 bits.  Certain cases require that both of these techniques are used.
  //
  // Example 1: b_min = 0x72, b_max = 0x7e.  The range is 0x0c.  This can be
  // encoded using delta_bits = 4 and overlap_bits = 0, which allows us to
  // represent an offset of 0x70 and a maximum delta of 0x0f, so that we can
  // encode values up to 0x7f.
  //
  // Example 2: b_min = 0x78, b_max = 0x84.  The range is 0x0c, but in this
  // case it is not sufficient to use delta_bits = 4 and overlap_bits = 0
  // because we can again only represent an offset of 0x70, so the maximum
  // delta of 0x0f only lets us encode values up to 0x7f.  However if we
  // increase the overlap to 4 bits then we can represent an offset of 0x78,
  // which lets us encode values up to 0x78 + 0x0f = 0x87.
  //
  // Example 3: b_min = 0x08, b_max = 0x104.  The range is 0xfc, so we should
  // be able to use 8-bit deltas.  But even with a 4-bit overlap, we can still
  // only encode offset = 0 and a maximum value of 0xff.  (We don't allow
  // bigger overlaps because statistically they are not worthwhile.)  Instead
  // we increase the delta size to 12 bits, which handles this case easily.
  //
  // Example 4: b_min = 0xf08, b_max = 0x1004.  The range is 0xfc, so we
  // should be able to use 8-bit deltas.  With 8-bit deltas and no overlap, we
  // have offset = 0xf00 and a maximum encodable value of 0xfff.  With 8-bit
  // deltas and a 4-bit overlap, we still have offset = 0xf00 and a maximum
  // encodable value of 0xfff.  Even with 12-bit deltas, we have offset = 0
  // and we can still only represent 0xfff.  However with delta_bits = 12 and
  // overlap_bits = 4, we can represent offset = 0xf00 and a maximum encodable
  // value of 0xf00 + 0xfff = 0x1eff.
  //
  // It is possible to show that this last example is the worst case, i.e.  we
  // do not need to consider increasing delta_bits or overlap_bits further.
  int delta_bits = (max(1, Bits::Log2Floor64(b_max - b_min)) + 3) & ~3;
  int overlap_bits = 0;
  if (!CanEncode(b_min, b_max, delta_bits, 0, have_exceptions)) {
    if (CanEncode(b_min, b_max, delta_bits, 4, have_exceptions)) {
      overlap_bits = 4;
    } else {
      S2_DCHECK_LE(delta_bits, 60);
      delta_bits += 4;
      if (!CanEncode(b_min, b_max, delta_bits, 0, have_exceptions)) {
        S2_DCHECK(CanEncode(b_min, b_max, delta_bits, 4, have_exceptions));
        overlap_bits = 4;
      }
    }
  }

  // When the block size is 1 and no exceptions exist, we have delta_bits == 4
  // and overlap_bits == 0 which wastes 4 bits.  We fix this below, which
  // among other things reduces the encoding size for single leaf cells by one
  // byte.  (Note that when exceptions exist, delta_bits == 8 and overlap_bits
  // may be 0 or 4.  These cases are covered by the unit tests.)
  if (values.size() == 1 && !have_exceptions) {
    S2_DCHECK(delta_bits == 4 && overlap_bits == 0);
    delta_bits = 8;
  }

  // Now determine the number of bytes needed to encode "offset", given the
  // chosen delta length.
  uint64 max_delta = BitMask(delta_bits) - (have_exceptions ? kBlockSize : 0);
  int offset_bits = 0;
  if (b_max > max_delta) {
    // At least one byte of offset is required.  Round up the minimum offset
    // to the next encodable value, and determine how many bits it has.
    int offset_shift = delta_bits - overlap_bits;
    uint64 mask = BitMask(offset_shift);
    uint64 min_offset = (b_max - max_delta + mask) & ~mask;
    S2_DCHECK_GT(min_offset, 0);
    offset_bits =
        (Bits::FindMSBSetNonZero64(min_offset) + 1 - offset_shift + 7) & ~7;
    // A 64-bit offset can only be encoded with an overlap of 4 bits.
    if (offset_bits == 64) overlap_bits = 4;
  }
  return BlockCode{delta_bits, offset_bits, overlap_bits};
}

bool EncodedS2PointVector::InitCellIdsFormat(Decoder* decoder) {
  // This function inverts the encodings documented above.
  // First we decode the two-byte header.
  if (decoder->avail() < 2) return false;
  uint8 header1 = decoder->get8();
  uint8 header2 = decoder->get8();
  S2_DCHECK_EQ(header1 & 7, CELL_IDS);
  int last_block_count, base_bytes;
  cell_ids_.have_exceptions = (header1 & 8) != 0;
  last_block_count = (header1 >> 4) + 1;
  base_bytes = header2 & 7;
  cell_ids_.level = header2 >> 3;

  // Decode the base value (if any).
  uint64 base;
  if (!DecodeUintWithLength(base_bytes, decoder, &base)) return false;
  cell_ids_.base = base << BaseShift(cell_ids_.level, base_bytes << 3);

  // Initialize the vector of encoded blocks.
  if (!cell_ids_.blocks.Init(decoder)) return false;
  size_ = kBlockSize * (cell_ids_.blocks.size() - 1) + last_block_count;
  return true;
}

S2Point EncodedS2PointVector::DecodeCellIdsFormat(int i) const {
  // This function inverts the encodings documented above.

  // First we decode the block header.
  const char* ptr = cell_ids_.blocks.GetStart(i >> kBlockShift);
  uint8 header = *ptr++;
  int overlap_nibbles = (header >> 3) & 1;
  int offset_bytes = (header & 7) + overlap_nibbles;
  int delta_nibbles = (header >> 4) + 1;

  // Decode the offset for this block.
  int offset_shift = (delta_nibbles - overlap_nibbles) << 2;
  uint64 offset = GetUintWithLength<uint64>(ptr, offset_bytes) << offset_shift;
  ptr += offset_bytes;

  // Decode the delta for the requested value.
  int delta_nibble_offset = (i & (kBlockSize - 1)) * delta_nibbles;
  int delta_bytes = (delta_nibbles + 1) >> 1;
  const char* delta_ptr = ptr + (delta_nibble_offset >> 1);
  uint64 delta = GetUintWithLength<uint64>(delta_ptr, delta_bytes);
  delta >>= (delta_nibble_offset & 1) << 2;
  delta &= BitMask(delta_nibbles << 2);

  // Test whether this point is encoded as an exception.
  if (cell_ids_.have_exceptions) {
    if (delta < kBlockSize) {
      int block_size = min(kBlockSize, size_ - (i & ~(kBlockSize - 1)));
      ptr += (block_size * delta_nibbles + 1) >> 1;
      ptr += delta * sizeof(S2Point);
      return *reinterpret_cast<const S2Point*>(ptr);
    }
    delta -= kBlockSize;
  }

  // Otherwise convert the 64-bit value back to an S2Point.
  uint64 value = cell_ids_.base + offset + delta;
  int shift = S2CellId::kMaxLevel - cell_ids_.level;

  // The S2CellId version of the following code is:
  //   return S2CellId(((value << 1) | 1) << (2 * shift)).ToPoint();
  uint32 sj, tj;
  DeinterleaveUint32BitPairs(value, &sj, &tj);
  int si = (((sj << 1) | 1) << shift) & 0x7fffffff;
  int ti = (((tj << 1) | 1) << shift) & 0x7fffffff;
  int face = ((sj << shift) >> 30) | (((tj << (shift + 1)) >> 29) & 4);
  return S2::FaceUVtoXYZ(face, S2::STtoUV(S2::SiTitoST(si)),
                         S2::STtoUV(S2::SiTitoST(ti))).Normalize();
}

}  // namespace s2coding
