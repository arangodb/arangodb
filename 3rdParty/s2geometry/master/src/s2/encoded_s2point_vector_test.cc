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

#include <vector>

#include <gtest/gtest.h>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"

#include "s2/base/log_severity.h"
#include "s2/util/bits/bit-interleave.h"
#include "s2/s2loop.h"
#include "s2/s2polygon.h"
#include "s2/s2shape.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2textformat::MakeCellIdOrDie;
using s2textformat::MakePointOrDie;
using std::vector;

namespace s2coding {

static int kBlockSize = 16;  // Number of deltas per block in implementation.

size_t TestEncodedS2PointVector(const vector<S2Point>& expected,
                                CodingHint hint, int64 expected_bytes) {
  Encoder encoder;
  EncodeS2PointVector(expected, hint, &encoder);
  if (expected_bytes >= 0) {
    EXPECT_EQ(expected_bytes, encoder.length());
  }
  Decoder decoder(encoder.base(), encoder.length());
  EncodedS2PointVector actual;
  EXPECT_TRUE(actual.Init(&decoder));
  EXPECT_EQ(actual.Decode(), expected);
  return encoder.length();
}

// In order to make it easier to construct tests that encode particular
// values, this function duplicates the part of EncodedS2PointVector that
// converts an encoded 64-bit value back to an S2Point.
S2Point EncodedValueToPoint(uint64 value, int level) {
  uint32 sj, tj;
  util_bits::DeinterleaveUint32(value, &sj, &tj);
  int shift = S2CellId::kMaxLevel - level;
  int si = (((sj << 1) | 1) << shift) & 0x7fffffff;
  int ti = (((tj << 1) | 1) << shift) & 0x7fffffff;
  int face = ((sj << shift) >> 30) | (((tj << (shift + 1)) >> 29) & 4);
  return S2::FaceUVtoXYZ(face, S2::STtoUV(S2::SiTitoST(si)),
                         S2::STtoUV(S2::SiTitoST(ti))).Normalize();
}

TEST(EncodedS2PointVectorTest, Empty) {
  TestEncodedS2PointVector({}, CodingHint::FAST, 1);

  // Test that an empty vector uses the UNCOMPRESSED encoding.
  TestEncodedS2PointVector({}, CodingHint::COMPACT, 1);
}

TEST(EncodedS2PointVectorTest, OnePoint) {
  TestEncodedS2PointVector({S2Point(1, 0, 0)}, CodingHint::FAST, 25);

  // Encoding: header (2 bytes), block count (1 byte), block offsets (1 byte),
  // block header (1 byte), delta (1 byte).
  TestEncodedS2PointVector({S2Point(1, 0, 0)}, CodingHint::COMPACT, 6);
}

TEST(EncodedS2PointVectorTest, OnePointWithExceptionsNoOverlap) {
  // Test encoding a block with one point when other blocks have exceptions
  // (which changes the encoding for all blocks).  The case below yields
  // delta_bits == 8 and overlap_bits == 0.
  //
  // Encoding: header (2 bytes), block count (1 byte), block offsets (2 bytes)
  // Block 0: block header (1 byte), 16 deltas (16 bytes), exception (24 bytes)
  // Block 1: block header (1 byte), delta (1 byte)
  S2Point a(1, 0, 0);
  vector<S2Point> points = {
    S2Point(1, 2, 3).Normalize(), a, a, a, a, a, a, a, a, a, a, a, a, a, a, a,
    a  // Second block
  };
  TestEncodedS2PointVector(points, CodingHint::COMPACT, 48);
}

TEST(EncodedS2PointVectorTest, OnePointWithExceptionsWithOverlap) {
  // Test encoding a block with one point when other blocks have exceptions
  // (which changes the encoding for all blocks).  The case below yields
  // delta_bits == 8 and overlap_bits == 4.
  //
  // Encoding: header (2 bytes), base (2 bytes), block count (1 byte),
  //           block offsets (2 bytes)
  // Block 0: header (1 byte), offset (2 bytes), 16 deltas (16 bytes),
  //          exception (24 bytes)
  // Block 1: header (1 byte), offset (2 bytes), delta (1 byte)
  S2Point a = S2CellId(0x946df618d0000000).ToPoint();
  S2Point b = S2CellId(0x947209e070000000).ToPoint();
  vector<S2Point> points = {
    S2Point(1, 2, 3).Normalize(), a, a, a, a, a, a, a, a, a, a, a, a, a, a, a,
    b  // Second block
  };
  TestEncodedS2PointVector(points, CodingHint::COMPACT, 54);
}

TEST(EncodedS2PointVectorTest, CellIdWithException) {
  // Test one point encoded as an S2CellId with one point encoded as an
  // exception.
  //
  // Encoding: header (2 bytes), block count (1 byte), block offsets (1 byte),
  // block header (1 byte), two deltas (2 bytes), exception (24 bytes).
  TestEncodedS2PointVector(
      {MakeCellIdOrDie("1/23").ToPoint(), S2Point(0.1, 0.2, 0.3).Normalize()},
      CodingHint::COMPACT, 31);
}

TEST(EncodedS2PointVectorTest, PointsAtMultipleLevels) {
  // Test that when points at multiple levels are present, the level with the
  // most points is chosen (preferring the smallest level in case of ties).
  // (All other points are encoded as exceptions.)

  // In this example, the two points at level 5 (on face 1) should be encoded.
  // It is possible to tell which points are encoded by the length of the
  // encoding (since different numbers of "base" bytes are encoded).
  //
  // Encoding: header (2 bytes), base (1 byte), block count (1 byte), block
  // offsets (1 byte), block header (1 byte), 5 deltas (5 bytes), S2Point
  // exceptions (72 bytes).
  TestEncodedS2PointVector(
      {MakeCellIdOrDie("2/11001310230102").ToPoint(),
       MakeCellIdOrDie("1/23322").ToPoint(),
       MakeCellIdOrDie("3/3").ToPoint(),
       MakeCellIdOrDie("1/23323").ToPoint(),
       MakeCellIdOrDie("2/12101023022012").ToPoint()},
      CodingHint::COMPACT, 83);
}

TEST(EncodedS2PointVectorTest, NoOverlapOrExtraDeltaBitsNeeded) {
  // This function tests the case in GetBlockCodes() where values can be
  // encoded using the minimum number delta bits and no overlap.  From the
  // comments there:
  //
  //   Example 1: d_min = 0x72, d_max = 0x7e.  The range is 0x0c.  This can be
  //   encoded using delta_bits = 4 and overlap_bits = 0, which allows us to
  //   represent an offset of 0x70 and a maximum delta of 0x0f, so that we can
  //   encode values up to 0x7f.
  //
  // To set up this test, we need at least two blocks: one to set the global
  // minimum value, and the other to encode a specific range of deltas.  To
  // make things easier, the first block has a minimum value of zero.
  //
  // Encoding: header (2 bytes), block count (1 byte), block offsets (2 bytes)
  // Block 0: header (1 byte), 8 deltas (8 bytes)
  // Block 1: header (1 byte), offset (1 byte), 4 deltas (2 bytes)
  const int level = 3;
  vector<S2Point> points(kBlockSize, EncodedValueToPoint(0, level));
  points.push_back(EncodedValueToPoint(0x72, level));
  points.push_back(EncodedValueToPoint(0x74, level));
  points.push_back(EncodedValueToPoint(0x75, level));
  points.push_back(EncodedValueToPoint(0x7e, level));
  TestEncodedS2PointVector(points, CodingHint::COMPACT, 10 + kBlockSize / 2);
}

TEST(EncodedS2PointVectorTest, OverlapNeeded) {
  // Like the above, but tests the following case:
  //
  //   Example 2: d_min = 0x78, d_max = 0x84.  The range is 0x0c, but in this
  //   case it is not sufficient to use delta_bits = 4 and overlap_bits = 0
  //   because we can again only represent an offset of 0x70, so the maximum
  //   delta of 0x0f only lets us encode values up to 0x7f.  However if we
  //   increase the overlap to 4 bits then we can represent an offset of 0x78,
  //   which lets us encode values up to 0x78 + 0x0f = 0x87.
  //
  // Encoding: header (2 bytes), block count (1 byte), block offsets (2 bytes)
  // Block 0: header (1 byte), 8 deltas (8 bytes)
  // Block 1: header (1 byte), offset (1 byte), 4 deltas (2 bytes)
  const int level = 3;
  vector<S2Point> points(kBlockSize, EncodedValueToPoint(0, level));
  points.push_back(EncodedValueToPoint(0x78, level));
  points.push_back(EncodedValueToPoint(0x7a, level));
  points.push_back(EncodedValueToPoint(0x7c, level));
  points.push_back(EncodedValueToPoint(0x84, level));
  TestEncodedS2PointVector(points, CodingHint::COMPACT, 10 + kBlockSize / 2);
}

TEST(EncodedS2PointVectorTest, ExtraDeltaBitsNeeded) {
  // Like the above, but tests the following case:
  //
  //   Example 3: d_min = 0x08, d_max = 0x104.  The range is 0xfc, so we should
  //   be able to use 8-bit deltas.  But even with a 4-bit overlap, we can still
  //   only encode offset = 0 and a maximum value of 0xff.  (We don't allow
  //   bigger overlaps because statistically they are not worthwhile.)  Instead
  //   we increase the delta size to 12 bits, which handles this case easily.
  //
  // Encoding: header (2 bytes), block count (1 byte), block offsets (2 bytes)
  // Block 0: header (1 byte), 8 deltas (8 bytes)
  // Block 1: header (1 byte), 4 deltas (6 bytes)
  const int level = 3;
  vector<S2Point> points(kBlockSize, EncodedValueToPoint(0, level));
  points.push_back(EncodedValueToPoint(0x08, level));
  points.push_back(EncodedValueToPoint(0x4e, level));
  points.push_back(EncodedValueToPoint(0x82, level));
  points.push_back(EncodedValueToPoint(0x104, level));
  TestEncodedS2PointVector(points, CodingHint::COMPACT, 13 + kBlockSize / 2);
}

TEST(EncodedS2PointVectorTest, ExtraDeltaBitsAndOverlapNeeded) {
  // Like the above, but tests the following case:
  //
  //   Example 4: d_min = 0xf08, d_max = 0x1004.  The range is 0xfc, so we
  //   should be able to use 8-bit deltas.  With 8-bit deltas and no overlap, we
  //   have offset = 0xf00 and a maximum encodable value of 0xfff.  With 8-bit
  //   deltas and a 4-bit overlap, we still have offset = 0xf00 and a maximum
  //   encodable value of 0xfff.  Even with 12-bit deltas, we have offset = 0
  //   and we can still only represent 0xfff.  However with delta_bits = 12 and
  //   overlap_bits = 4, we can represent offset = 0xf00 and a maximum encodable
  //   value of 0xf00 + 0xfff = 0x1eff.
  //
  // Encoding: header (2 bytes), block count (1 byte), block offsets (2 bytes)
  // Block 0: header (1 byte), 8 deltas (8 bytes)
  // Block 1: header (1 byte), offset (1 byte), 4 deltas (6 bytes)
  const int level = 5;
  vector<S2Point> points(kBlockSize, EncodedValueToPoint(0, level));
  points.push_back(EncodedValueToPoint(0xf08, level));
  points.push_back(EncodedValueToPoint(0xf4e, level));
  points.push_back(EncodedValueToPoint(0xf82, level));
  points.push_back(EncodedValueToPoint(0x1004, level));
  TestEncodedS2PointVector(points, CodingHint::COMPACT, 14 + kBlockSize / 2);
}

TEST(EncodedS2PointVectorTest, SixtyFourBitOffset) {
  // Tests a case where a 64-bit block offset is needed.
  //
  // Encoding: header (2 bytes), block count (1 byte), block offsets (2 bytes)
  // Block 0: header (1 byte), 8 deltas (8 bytes)
  // Block 1: header (1 byte), offset (8 bytes), 2 deltas (1 byte)
  const int level = S2CellId::kMaxLevel;
  vector<S2Point> points(kBlockSize, S2CellId::Begin(level).ToPoint());
  points.push_back(S2CellId::End(level).prev().ToPoint());
  points.push_back(S2CellId::End(level).prev().prev().ToPoint());
  TestEncodedS2PointVector(points, CodingHint::COMPACT, 16 + kBlockSize / 2);
}

TEST(EncodedS2PointVectorTest, AllExceptionsBlock) {
  // The encoding consists of two blocks; the first contains 16 encodable
  // values, while the second contains two exceptions.
  vector<S2Point> points(kBlockSize,
                         EncodedValueToPoint(0, S2CellId::kMaxLevel));
  points.push_back(S2Point(0.1, 0.2, 0.3).Normalize());
  points.push_back(S2Point(0.3, 0.2, 0.1).Normalize());
  // Encoding: header (2 bytes), block count (1 byte), block offsets (2 bytes).
  // 1st block header (1 byte), 16 deltas (16 bytes).
  // 2nd block header (1 byte), 2 deltas (1 byte), 2 exceptions (48 bytes).
  TestEncodedS2PointVector(points, CodingHint::COMPACT, 72);

  // Encoding: header (2 bytes), 18 S2Points (432 bytes).
  TestEncodedS2PointVector(points, CodingHint::FAST, 434);
}

TEST(EncodedS2PointVectorTest, FirstAtAllLevels) {
  // Test encoding the first S2CellId at each level (which also happens to have
  // the maximum face, si, and ti values).  All such S2CellIds can be encoded in
  // 6 bytes because most of the bits are zero.
  for (int level = 0; level <= S2CellId::kMaxLevel; ++level) {
    SCOPED_TRACE(absl::StrCat("Level = ", level));
    TestEncodedS2PointVector({S2CellId::Begin(level).ToPoint()},
                             CodingHint::COMPACT, 6);
  }
}

TEST(EncodedS2PointVectorTest, LastAtAllLevels) {
  // Test encoding the last S2CellId at each level.  It turns out that such
  // S2CellIds have the largest possible face and ti values, and the minimum
  // possible si value at that level.  Such S2CellIds can be encoded in 6 to 13
  // bytes depending on the level.
  for (int level = 0; level <= S2CellId::kMaxLevel; ++level) {
    SCOPED_TRACE(absl::StrCat("Level = ", level));
    // Note that 8 bit deltas are used to encode blocks of size 1, which
    // reduces the size of "base" from ((level + 2) / 4) to (level / 4) bytes.
    int expected_size = 6 + level / 4;
    TestEncodedS2PointVector({S2CellId::End(level).prev().ToPoint()},
                             CodingHint::COMPACT, expected_size);
  }
}

TEST(EncodedS2PointVectorTest, MaxFaceSiTiAtAllLevels) {
  // Similar to the test above, but tests encoding the S2CellId at each level
  // whose face, si, and ti values are all maximal.  This turns out to be the
  // S2CellId whose human-readable form is 5/222...22 (0xb555555555555555),
  // however for clarity we consruct it using S2CellId::FromFaceIJ.
  for (int level = 0; level <= S2CellId::kMaxLevel; ++level) {
    SCOPED_TRACE(absl::StrCat("Level = ", level));
    S2CellId id = S2CellId::FromFaceIJ(5, S2::kLimitIJ - 1, S2::kLimitIJ - 1)
                  .parent(level);

    // This encoding is one byte bigger than the previous test at levels 7, 11,
    // 15, 19, 23, and 27.  This is because in the previous test, the
    // odd-numbered value bits are all zero (except for the face number), which
    // reduces the number of base bits needed by exactly 1.  The encoding size
    // at level==3 is unaffected because for singleton blocks, the lowest 8
    // value bits are encoded in the delta.
    int expected_size = (level < 4) ? 6 : 6 + (level + 1) / 4;
    TestEncodedS2PointVector({id.ToPoint()},
                             CodingHint::COMPACT, expected_size);
  }
}

TEST(EncodedS2PointVectorTest, LastTwoPointsAtAllLevels) {
  // Test encoding the last two S2CellIds at each level.
  for (int level = 0; level <= S2CellId::kMaxLevel; ++level) {
    SCOPED_TRACE(absl::StrCat("Level = ", level));
    S2CellId id = S2CellId::End(level).prev();
    // Notice that this costs only 4 bits more than encoding the last S2CellId
    // by itself (see LastAtAllLevels).  This is because encoding a block of
    // size 1 uses 8-bit deltas (which reduces the size of "base" by 4 bits),
    // while this test uses two 4-bit deltas.
    int expected_size = 6 + (level + 2) / 4;
    TestEncodedS2PointVector({id.ToPoint(), id.prev().ToPoint()},
                             CodingHint::COMPACT, expected_size);
  }
}

TEST(EncodedS2PointVectorTest, ManyDuplicatePointsAtAllLevels) {
  // Test encoding 32 copies of the last S2CellId at each level.  This uses
  // between 27 and 38 bytes depending on the level.  (Note that the encoding
  // can use less than 1 byte per point in this situation.)
  for (int level = 0; level <= S2CellId::kMaxLevel; ++level) {
    SCOPED_TRACE(absl::StrCat("Level = ", level));
    S2CellId id = S2CellId::End(level).prev();
    // Encoding: header (2 bytes), base ((level + 2) / 4 bytes), block count
    // (1 byte), block offsets (2 bytes), block headers (2 bytes), 32 deltas
    // (16 bytes).  At level 30 the encoding size goes up by 1 byte because
    // we can't encode an 8 byte "base" value, so instead this case uses a
    // base of 7 bytes plus a one-byte offset in each of the 2 blocks.
    int expected_size = 23 + (level + 2) / 4;
    if (level == 30) expected_size += 1;
    vector<S2Point> points(32, id.ToPoint());
    TestEncodedS2PointVector(points, CodingHint::COMPACT, expected_size);
  }
}

TEST(EncodedS2PointVectorTest, SnappedFractalLoops) {
  S2Testing::rnd.Reset(absl::GetFlag(FLAGS_s2_random_seed));
  int kMaxPoints = 3 << (google::DEBUG_MODE ? 10 : 14);
  for (int num_points = 3; num_points <= kMaxPoints; num_points *= 4) {
    size_t s2polygon_size = 0, lax_polygon_size = 0;
    for (int i = 0; i < 10; ++i) {
      S2Testing::Fractal fractal;
      fractal.SetLevelForApproxMaxEdges(num_points);
      auto frame = S2Testing::GetRandomFrame();
      auto loop = fractal.MakeLoop(frame, S2Testing::KmToAngle(10));
      std::vector<S2Point> points;
      for (int j = 0; j < loop->num_vertices(); ++j) {
        points.push_back(S2CellId(loop->vertex(j)).ToPoint());
      }
      S2Polygon s2polygon(make_unique<S2Loop>(points));
      Encoder encoder;
      s2polygon.Encode(&encoder);
      s2polygon_size += encoder.length();
      // S2LaxPolygonShape has 2 extra bytes of overhead to encode one loop.
      lax_polygon_size +=
          TestEncodedS2PointVector(points, CodingHint::COMPACT, -1) + 2;
    }
    printf("n=%5d  s2=%9" PRIuS "  lax=%9" PRIuS "\n",
           num_points, s2polygon_size, lax_polygon_size);
  }
}

void TestRoundtripEncoding(s2coding::CodingHint hint) {
  // Ensures that the EncodedS2PointVector can be encoded and decoded without
  // loss.
  const int level = 3;
  vector<S2Point> points(kBlockSize, EncodedValueToPoint(0, level));
  points.push_back(EncodedValueToPoint(0x78, level));
  points.push_back(EncodedValueToPoint(0x7a, level));
  points.push_back(EncodedValueToPoint(0x7c, level));
  points.push_back(EncodedValueToPoint(0x84, level));

  EncodedS2PointVector a_vector;
  Encoder a_encoder;

  EncodedS2PointVector b_vector;
  Encoder b_encoder;

  // Encode and decode from a vector<S2Point>.
  {
    EncodeS2PointVector(points, hint, &a_encoder);
    Decoder decoder(a_encoder.base(), a_encoder.length());
    a_vector.Init(&decoder);
  }
  ASSERT_EQ(points, a_vector.Decode());

  // Encode and decode from an EncodedS2PointVector.
  {
    a_vector.Encode(&b_encoder);
    Decoder decoder(b_encoder.base(), b_encoder.length());
    b_vector.Init(&decoder);
  }
  EXPECT_EQ(points, b_vector.Decode());
}

TEST(EncodedS2PointVectorTest, RoundtripEncodingFast) {
  TestRoundtripEncoding(s2coding::CodingHint::FAST);
}

TEST(EncodedS2PointVectorTest, RoundtripEncodingCompact) {
  TestRoundtripEncoding(s2coding::CodingHint::COMPACT);
}

}  // namespace s2coding
