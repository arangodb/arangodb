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

#include <vector>
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2loop.h"
#include "s2/s2pointutil.h"
#include "s2/s2shape_index.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2textformat::MakeCellIdOrDie;
using std::vector;

namespace s2coding {

// Encodes the given vector and returns the corresponding
// EncodedS2CellIdVector (which points into the Encoder's data buffer).
EncodedS2CellIdVector MakeEncodedS2CellIdVector(const vector<S2CellId>& input,
                                                Encoder* encoder) {
  EncodeS2CellIdVector(input, encoder);
  Decoder decoder(encoder->base(), encoder->length());
  EncodedS2CellIdVector cell_ids;
  EXPECT_TRUE(cell_ids.Init(&decoder));
  return cell_ids;
}

// Encodes the given vector and checks that it has the expected size and
// contents.
void TestEncodedS2CellIdVector(const vector<S2CellId>& expected,
                               size_t expected_bytes) {
  Encoder encoder;
  EncodedS2CellIdVector actual = MakeEncodedS2CellIdVector(expected, &encoder);
  EXPECT_EQ(expected_bytes, encoder.length());
  EXPECT_EQ(actual.Decode(), expected);
}

// Like the above, but accepts a vector<uint64> rather than a vector<S2CellId>.
void TestEncodedS2CellIdVector(const vector<uint64>& raw_expected,
                               size_t expected_bytes) {
  vector<S2CellId> expected;
  for (uint64 raw_id : raw_expected) {
    expected.push_back(S2CellId(raw_id));
  }
  TestEncodedS2CellIdVector(expected, expected_bytes);
}

TEST(EncodedS2CellIdVector, Empty) {
  TestEncodedS2CellIdVector(vector<S2CellId>{}, 2);
}

TEST(EncodedS2CellIdVector, None) {
  TestEncodedS2CellIdVector({S2CellId::None()}, 3);
}

TEST(EncodedS2CellIdVector, NoneNone) {
  TestEncodedS2CellIdVector({S2CellId::None(), S2CellId::None()}, 4);
}

TEST(EncodedS2CellIdVector, Sentinel) {
  TestEncodedS2CellIdVector({S2CellId::Sentinel()}, 10);
}

TEST(EncodedS2CellIdVector, MaximumShiftCell) {
  // Tests the encoding of a single cell at level 2, which corresponds the
  // maximum encodable shift value (56).
  TestEncodedS2CellIdVector({MakeCellIdOrDie("0/00")}, 3);
}

TEST(EncodedS2CellIdVector, SentinelSentinel) {
  TestEncodedS2CellIdVector({S2CellId::Sentinel(), S2CellId::Sentinel()}, 11);
}

TEST(EncodedS2CellIdVector, NoneSentinelNone) {
  TestEncodedS2CellIdVector(
      {S2CellId::None(), S2CellId::Sentinel(), S2CellId::None()}, 26);
}

TEST(EncodedS2CellIdVector, InvalidCells) {
  // Tests that cells with an invalid LSB can be encoded.
  TestEncodedS2CellIdVector({0x6, 0xe, 0x7e}, 5);
}

TEST(EncodedS2CellIdVector, OneByteLeafCells) {
  // Tests that (1) if all cells are leaf cells, the low bit is not encoded,
  // and (2) this can be indicated using the standard 1-byte header.
  TestEncodedS2CellIdVector({0x3, 0x7, 0x177}, 5);
}

TEST(EncodedS2CellIdVector, OneByteLevel29Cells) {
  // Tests that (1) if all cells are at level 29, the low bit is not encoded,
  // and (2) this can be indicated using the standard 1-byte header.
  TestEncodedS2CellIdVector({0xc, 0x1c, 0x47c}, 5);
}

TEST(EncodedS2CellIdVector, OneByteLevel28Cells) {
  // Tests that (1) if all cells are at level 28, the low bit is not encoded,
  // and (2) this can be indicated using the extended 2-byte header.
  TestEncodedS2CellIdVector({0x30, 0x70, 0x1770}, 6);
}

TEST(EncodedS2CellIdVector, OneByteMixedCellLevels) {
  // Tests that cells at mixed levels can be encoded in one byte.
  TestEncodedS2CellIdVector({0x300, 0x1c00, 0x7000, 0xff00}, 6);
}

TEST(EncodedS2CellIdVector, OneByteMixedCellLevelsWithPrefix) {
  // Tests that cells at mixed levels can be encoded in one byte even when
  // they share a multi-byte prefix.
  TestEncodedS2CellIdVector({
      0x1234567800000300, 0x1234567800001c00,
      0x1234567800007000, 0x123456780000ff00}, 10);
}

TEST(EncodedS2CellIdVector, OneByteRangeWithBaseValue) {
  // Tests that cells can be encoded in one byte by choosing a base value
  // whose bit range overlaps the delta values.
  // 1 byte header, 3 bytes base, 1 byte size, 4 bytes deltas
  TestEncodedS2CellIdVector({
      0x00ffff0000000000, 0x0100fc0000000000,
      0x0100500000000000, 0x0100330000000000}, 9);
}

TEST(EncodedS2CellIdVector, SixFaceCells) {
  vector<S2CellId> ids;
  for (int face = 0; face < 6; ++face) {
    ids.push_back(S2CellId::FromFace(face));
  }
  TestEncodedS2CellIdVector(ids, 8);
}

TEST(EncodedS2CellIdVector, FourLevel10Children) {
  vector<S2CellId> ids;
  S2CellId parent = MakeCellIdOrDie("3/012301230");
  for (S2CellId id = parent.child_begin();
       id != parent.child_end(); id = id.next()) {
    ids.push_back(id);
  }
  TestEncodedS2CellIdVector(ids, 8);
}

TEST(EncodedS2CellIdVector, FractalS2ShapeIndexCells) {
  S2Testing::Fractal fractal;
  fractal.SetLevelForApproxMaxEdges(3 * 1024);
  S2Point center = s2textformat::MakePointOrDie("47.677:-122.206");
  MutableS2ShapeIndex index;
  index.Add(make_unique<S2Loop::OwningShape>(
      fractal.MakeLoop(S2::GetFrame(center), S1Angle::Degrees(1))));
  vector<S2CellId> ids;
  for (MutableS2ShapeIndex::Iterator it(&index, S2ShapeIndex::BEGIN);
       !it.done(); it.Next()) {
    ids.push_back(it.id());
  }
  EXPECT_EQ(966, ids.size());
  TestEncodedS2CellIdVector(ids, 2902);
}

TEST(EncodedS2CellIdVector, CoveringCells) {
  vector<uint64> ids {
    0x414a617f00000000, 0x414a61c000000000, 0x414a624000000000,
    0x414a63c000000000, 0x414a647000000000, 0x414a64c000000000,
    0x414a653000000000, 0x414a704000000000, 0x414a70c000000000,
    0x414a714000000000, 0x414a71b000000000, 0x414a7a7c00000000,
    0x414a7ac000000000, 0x414a8a4000000000, 0x414a8bc000000000,
    0x414a8c4000000000, 0x414a8d7000000000, 0x414a8dc000000000,
    0x414a914000000000, 0x414a91c000000000, 0x414a924000000000,
    0x414a942c00000000, 0x414a95c000000000, 0x414a96c000000000,
    0x414ab0c000000000, 0x414ab14000000000, 0x414ab34000000000,
    0x414ab3c000000000, 0x414ab44000000000, 0x414ab4c000000000,
    0x414ab6c000000000, 0x414ab74000000000, 0x414ab8c000000000,
    0x414ab94000000000, 0x414aba1000000000, 0x414aba3000000000,
    0x414abbc000000000, 0x414abe4000000000, 0x414abec000000000,
    0x414abf4000000000, 0x46b5454000000000, 0x46b545c000000000,
    0x46b5464000000000, 0x46b547c000000000, 0x46b5487000000000,
    0x46b548c000000000, 0x46b5494000000000, 0x46b54a5400000000,
    0x46b54ac000000000, 0x46b54b4000000000, 0x46b54bc000000000,
    0x46b54c7000000000, 0x46b54c8004000000, 0x46b54ec000000000,
    0x46b55ad400000000, 0x46b55b4000000000, 0x46b55bc000000000,
    0x46b55c4000000000, 0x46b55c8100000000, 0x46b55dc000000000,
    0x46b55e4000000000, 0x46b5604000000000, 0x46b560c000000000,
    0x46b561c000000000, 0x46ca424000000000, 0x46ca42c000000000,
    0x46ca43c000000000, 0x46ca444000000000, 0x46ca45c000000000,
    0x46ca467000000000, 0x46ca469000000000, 0x46ca5fc000000000,
    0x46ca604000000000, 0x46ca60c000000000, 0x46ca674000000000,
    0x46ca679000000000, 0x46ca67f000000000, 0x46ca684000000000,
    0x46ca855000000000, 0x46ca8c4000000000, 0x46ca8cc000000000,
    0x46ca8e5400000000, 0x46ca8ec000000000, 0x46ca8f0100000000,
    0x46ca8fc000000000, 0x46ca900400000000, 0x46ca98c000000000,
    0x46ca994000000000, 0x46ca99c000000000, 0x46ca9a4000000000,
    0x46ca9ac000000000, 0x46ca9bd500000000, 0x46ca9e4000000000,
    0x46ca9ec000000000, 0x46caf34000000000, 0x46caf4c000000000,
    0x46caf54000000000
  };
  EXPECT_EQ(97, ids.size());
  TestEncodedS2CellIdVector(ids, 488);
}

TEST(EncodedS2CellIdVector, LowerBoundLimits) {
  // Test seeking before the beginning and past the end of the vector.
  S2CellId first = S2CellId::Begin(S2CellId::kMaxLevel);
  S2CellId last = S2CellId::End(S2CellId::kMaxLevel).prev();
  Encoder encoder;
  EncodedS2CellIdVector cell_ids = MakeEncodedS2CellIdVector(
      {first, last}, &encoder);
  EXPECT_EQ(0, cell_ids.lower_bound(S2CellId::None()));
  EXPECT_EQ(0, cell_ids.lower_bound(first));
  EXPECT_EQ(1, cell_ids.lower_bound(first.next()));
  EXPECT_EQ(1, cell_ids.lower_bound(last.prev()));
  EXPECT_EQ(1, cell_ids.lower_bound(last));
  EXPECT_EQ(2, cell_ids.lower_bound(last.next()));
  EXPECT_EQ(2, cell_ids.lower_bound(S2CellId::Sentinel()));
}

}  // namespace s2coding
