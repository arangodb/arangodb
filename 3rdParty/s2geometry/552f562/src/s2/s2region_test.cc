// Copyright 2016 Google Inc. All Rights Reserved.
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


#include "s2/s2region.h"

#include <gtest/gtest.h>
#include "s2/third_party/absl/container/fixed_array.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2loop.h"
#include "s2/s2point_compression.h"
#include "s2/s2point_region.h"
#include "s2/s2pointutil.h"
#include "s2/s2polygon.h"
#include "s2/s2polyline.h"
#include "s2/s2r2rect.h"
#include "s2/s2region_intersection.h"
#include "s2/s2region_union.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using std::unique_ptr;
using std::vector;

namespace {

//////////////  These values are in version 1 encoding format.  ////////////////

// S2Cap.
const char kEncodedCapEmpty[] =
    "000000000000F03F00000000000000000000000000000000000000000000F0BF";
const char kEncodedCapFull[] =
    "000000000000F03F000000000000000000000000000000000000000000001040";
// S2Cap from S2Point(3, 2, 1).Normalize()
const char kEncodedCapFromPoint[] =
    "3F36105836A8E93F2A2460E5CE1AE13F2A2460E5CE1AD13F0000000000000000";
// S2Cap from S2Point(0, 0, 1) with height 5
const char kEncodedCapFromCenterHeight[] =
    "00000000000000000000000000000000000000000000F03F0000000000001040";

// S2CellId.
// S2CellId from Face 0.
const char kEncodedCellIDFace0[] = "0000000000000010";
// S2CellId from Face 5.
const char kEncodedCellIDFace5[] = "00000000000000B0";
// S2CellId from Face 0 in the last S2Cell at kMaxLevel.
const char kEncodedCellIDFace0MaxLevel[] = "0100000000000020";
// S2CellId from Face 5 in the last S2Cell at kMaxLevel.
const char kEncodedCellIDFace5MaxLevel[] = "01000000000000C0";
// S2CellId FromFacePosLevel(3, 0x12345678, S2CellId::kMaxLevel - 4)
const char kEncodedCellIDFacePosLevel[] = "0057341200000060";
// S2CellId from the 0 value.
const char kEncodedCellIDInvalid[] = "0000000000000000";

// S2Cell.
// S2Cell from S2Point(1, 2, 3)
const char kEncodedCellFromPoint[] = "F51392E0F35DCC43";
// S2Cell from LatLng(39.0, -120.0) - The Lake Tahoe border corner of CA/NV.
const char kEncodedCellFromLatLng[] = "6308962A95849980";
// S2Cell from FacePosLevel(3, 0x12345678, S2CellId::kMaxLevel - 4)
const char kEncodedCellFromFacePosLevel[] = "0057341200000060";
// S2Cell from Face 0.
const char kEncodedCellFace0[] = "0000000000000010";

// S2CellUnion.
// An unitialized empty S2CellUnion.
const char kEncodedCellUnionEmpty[] = "010000000000000000";
// S2CellUnion from an S2CellId from Face 1.
const char kEncodedCellUnionFace1[] = "0101000000000000000000000000000030";
// S2CellUnion from the cells {0x33, 0x8e3748fab, 0x91230abcdef83427};
const char kEncodedCellUnionFromCells[] =
    "0103000000000000003300000000000000AB8F74E3080000002734F8DEBC0A2391";

// S2LatLngRect
const char kEncodedRectEmpty[] =
    "01000000000000F03F0000000000000000182D4454FB210940182D4454FB2109C0";
const char kEncodedRectFull[] =
    "01182D4454FB21F9BF182D4454FB21F93F182D4454FB2109C0182D4454FB210940";
// S2LatLngRect from Center=(80,170), Size=(40,60)
const char kEncodedRectCentersize[] =
    "0165732D3852C1F03F182D4454FB21F93FF75B8A41358C03408744E74A185706C0";

// S2Loop
const char kEncodedLoopEmpty[] =
    "010100000000000000000000000000000000000000000000000000F03F0000000000010000"
    "00000000F03F0000000000000000182D4454FB210940182D4454FB2109C0";
const char kEncodedLoopFull[] =
    "010100000000000000000000000000000000000000000000000000F0BF010000000001182D"
    "4454FB21F9BF182D4454FB21F93F182D4454FB2109C0182D4454FB210940";
// S2Loop from the unit test value kCross1;
const char kEncodedLoopCross[] =
    "0108000000D44A8442C3F9EF3F7EDA2AB341DC913F27DCF7C958DEA1BFB4825F3C81FDEF3F"
    "27DCF7C958DE913F1EDD892B0BDF91BFB4825F3C81FDEF3F27DCF7C958DE913F1EDD892B0B"
    "DF913FD44A8442C3F9EF3F7EDA2AB341DC913F27DCF7C958DEA13FD44A8442C3F9EF3F7EDA"
    "2AB341DC91BF27DCF7C958DEA13FB4825F3C81FDEF3F27DCF7C958DE91BF1EDD892B0BDF91"
    "3FB4825F3C81FDEF3F27DCF7C958DE91BF1EDD892B0BDF91BFD44A8442C3F9EF3F7EDA2AB3"
    "41DC91BF27DCF7C958DEA1BF0000000000013EFC10E8F8DFA1BF3EFC10E8F8DFA13F389D52"
    "A246DF91BF389D52A246DF913F";
// S2Loop encoded using EncodeCompressed from snapped points.
// vector<S2Point> snapped_loop_a_vertices = {
//       S2CellId(s2textformat::MakePoint("0:178")).ToPoint(),
//       S2CellId(s2textformat::MakePoint("-1:180")).ToPoint(),
//       S2CellId(s2textformat::MakePoint("0:-179")).ToPoint(),
//       S2CellId(s2textformat::MakePoint("1:-180")).ToPoint()};
// snapped_loop = make_unique<S2Loop>(snapped_loop_a_vertices));
// absl::FixedArray<S2XYZFaceSiTi> points(loop.num_vertices());
// loop.GetXYZFaceSiTiVertices(points.data());
// loop.EncodeCompressed(encoder, points.data(), level);
//
const char kEncodedLoopCompressed[] =
    "041B02222082A222A806A0C7A991DE86D905D7C3A691F2DEE40383908880A095880500000"
    "3";

// S2PointRegion
// The difference between an S2PointRegion and an S2Point being encoded is the
// presence of the encoding version number as the first 8 bits. (i.e. when
// S2Loop and others encode a stream of S2Points, they are writing triples of
// doubles instead of encoding the points with the version byte.)
// S2PointRegion(S2::Origin())
const char kEncodedPointOrigin[] =
    "013BED86AA997A84BF88EC8B48C53C653FACD2721A90FFEF3F";
// S2PointRegion(S2Point(12.34, 56.78, 9.1011).Normalize())
const char kEncodedPointTesting[] =
    "0109AD578332DBCA3FBC9FDB9BB4E4EE3FE67E7C2CA7CEC33F";

// S2Polygon
// S2Polygon from s2textformat::MakePolygon("").
// This is encoded in compressed format v4.
const char kEncodedPolygonEmpty[] = "041E00";
// S2Polygon from s2textformat::MakePolygon("full").
// This is encoded in compressed format v4.
const char kEncodedPolygonFull[] = "040001010B000100";
// S2Polygon from the unit test value kCross1. Encoded in lossless format.
const char kEncodedPolygon1Loops[] =
    "010100010000000108000000D44A8442C3F9EF3F7EDA2AB341DC913F27DCF7C958DEA1BFB4"
    "825F3C81FDEF3F27DCF7C958DE913F1EDD892B0BDF91BFB4825F3C81FDEF3F27DCF7C958DE"
    "913F1EDD892B0BDF913FD44A8442C3F9EF3F7EDA2AB341DC913F27DCF7C958DEA13FD44A84"
    "42C3F9EF3F7EDA2AB341DC91BF27DCF7C958DEA13FB4825F3C81FDEF3F27DCF7C958DE91BF"
    "1EDD892B0BDF913FB4825F3C81FDEF3F27DCF7C958DE91BF1EDD892B0BDF91BFD44A8442C3"
    "F9EF3F7EDA2AB341DC91BF27DCF7C958DEA1BF0000000000013EFC10E8F8DFA1BF3EFC10E8"
    "F8DFA13F389D52A246DF91BF389D52A246DF913F013EFC10E8F8DFA1BF3EFC10E8F8DFA13F"
    "389D52A246DF91BF389D52A246DF913F";
// S2Polygon from the unit test value kCross1+kCrossHole.
// This is encoded in lossless format.
const char kEncodedPolygon2Loops[] =
    "010101020000000108000000D44A8442C3F9EF3F7EDA2AB341DC913F27DCF7C958DEA1BFB4"
    "825F3C81FDEF3F27DCF7C958DE913F1EDD892B0BDF91BFB4825F3C81FDEF3F27DCF7C958DE"
    "913F1EDD892B0BDF913FD44A8442C3F9EF3F7EDA2AB341DC913F27DCF7C958DEA13FD44A84"
    "42C3F9EF3F7EDA2AB341DC91BF27DCF7C958DEA13FB4825F3C81FDEF3F27DCF7C958DE91BF"
    "1EDD892B0BDF913FB4825F3C81FDEF3F27DCF7C958DE91BF1EDD892B0BDF91BFD44A8442C3"
    "F9EF3F7EDA2AB341DC91BF27DCF7C958DEA1BF0000000000013EFC10E8F8DFA1BF3EFC10E8"
    "F8DFA13F389D52A246DF91BF389D52A246DF913F0104000000C5D7FA4B60FFEF3F1EDD892B"
    "0BDF813F214C95C437DF81BFC5D7FA4B60FFEF3F1EDD892B0BDF813F214C95C437DF813FC5"
    "D7FA4B60FFEF3F1EDD892B0BDF81BF214C95C437DF813FC5D7FA4B60FFEF3F1EDD892B0BDF"
    "81BF214C95C437DF81BF000100000001900C5E3B73DF81BF900C5E3B73DF813F399D52A246"
    "DF81BF399D52A246DF813F013EFC10E8F8DFA1BF3EFC10E8F8DFA13F389D52A246DF91BF38"
    "9D52A246DF913F";
// TODO(user): Generate S2Polygons that use compressed encoding format for
// testing.

// S2Polyline
// An S2Polyline from an empty vector.
const char kEncodedPolylineEmpty[] = "0100000000";
// An S2Polyline from 3 S2LatLngs {(0, 0),(0, 90),(0,180)};
const char kEncodedPolylineSemiEquator[] =
    "0103000000000000000000F03F00000000000000000000000000000000075C143326A6913C"
    "000000000000F03F0000000000000000000000000000F0BF075C143326A6A13C0000000000"
    "000000";
// An S2Polyline from MakePolyline("0:0, 0:10, 10:20, 20:30");
const char kEncodedPolyline3Segments[] =
    "0104000000000000000000F03F00000000000000000000000000000000171C818C8B83EF3F"
    "89730B7E1A3AC63F000000000000000061B46C3A039DED3FE2DC829F868ED53F89730B7E1A"
    "3AC63F1B995E6FA10AEA3F1B2D5242F611DE3FF50B8A74A8E3D53F";

// Encode/Decode not yet implemented for these types.
// S2R2Rect
// S2RegionIntersection
// S2RegionUnion

//////////////////////////////////////////////////////////////

// HexEncodeStr returns the data in str in hex encoded form.
const string HexEncodeStr(const string& str) {
  static const char* const lut = "0123456789ABCDEF";

  string result;
  result.reserve(2 * str.size());
  for (size_t i = 0; i < str.length(); ++i) {
    const unsigned char c = str[i];
    result.push_back(lut[c >> 4]);
    result.push_back(lut[c & 15]);
  }
  return result;
}

class S2RegionEncodeDecodeTest : public testing::Test {
 protected:
  // TestEncodeDecode tests that the input encodes to match the expected
  // golden data, and then returns the decode of the data into dst.
  template <class Region>
  void TestEncodeDecode(const string& golden, const Region& src, Region* dst) {
    Encoder encoder;
    src.Encode(&encoder);

    EXPECT_EQ(golden, HexEncodeStr(string(encoder.base(), encoder.length())));

    Decoder decoder(encoder.base(), encoder.length());
    dst->Decode(&decoder);
  }
};

TEST_F(S2RegionEncodeDecodeTest, S2Cap) {
  S2Cap cap;
  S2Cap cap_empty = S2Cap::Empty();
  S2Cap cap_full = S2Cap::Full();
  S2Cap cap_from_point = S2Cap::FromPoint(S2Point(3, 2, 1).Normalize());
  S2Cap cap_from_center_height =
      S2Cap::FromCenterHeight(S2Point(0, 0, 1).Normalize(), 5);

  TestEncodeDecode(kEncodedCapEmpty, cap_empty, &cap);
  EXPECT_TRUE(cap_empty.ApproxEquals(cap));
  TestEncodeDecode(kEncodedCapFull, cap_full, &cap);
  EXPECT_TRUE(cap_full.ApproxEquals(cap));
  TestEncodeDecode(kEncodedCapFromPoint, cap_from_point, &cap);
  EXPECT_TRUE(cap_from_point.ApproxEquals(cap));
  TestEncodeDecode(kEncodedCapFromCenterHeight, cap_from_center_height, &cap);
  EXPECT_TRUE(cap_from_center_height.ApproxEquals(cap));
}

TEST_F(S2RegionEncodeDecodeTest, S2Cell) {
  S2Cell cell;
  S2Cell cell_from_point(S2Point(1, 2, 3));
  S2Cell cell_from_latlng(S2LatLng::FromDegrees(39.0, -120.0));
  S2Cell cell_from_face_pos_lvl(
      S2Cell::FromFacePosLevel(3, 0x12345678, S2CellId::kMaxLevel - 4));
  S2Cell cell_from_from_face(S2Cell::FromFace(0));

  TestEncodeDecode(kEncodedCellFromPoint, cell_from_point, &cell);
  EXPECT_EQ(cell_from_point, cell);
  TestEncodeDecode(kEncodedCellFromLatLng, cell_from_latlng, &cell);
  EXPECT_EQ(cell_from_latlng, cell);
  TestEncodeDecode(kEncodedCellFromFacePosLevel, cell_from_face_pos_lvl, &cell);
  EXPECT_EQ(cell_from_face_pos_lvl, cell);
  TestEncodeDecode(kEncodedCellFace0, cell_from_from_face, &cell);
  EXPECT_EQ(cell_from_from_face, cell);
}

TEST_F(S2RegionEncodeDecodeTest, S2CellUnion) {
  S2CellUnion cu;
  const S2CellUnion cu_empty;
  const S2CellUnion cu_face1({S2CellId::FromFace(1)});
  // Cell ids taken from S2CellUnion EncodeDecode test.
  const S2CellUnion cu_latlngs =
      S2CellUnion::FromNormalized({S2CellId(0x33),
                                   S2CellId(0x8e3748fab),
                                   S2CellId(0x91230abcdef83427)});

  TestEncodeDecode(kEncodedCellUnionEmpty, cu_empty, &cu);
  EXPECT_EQ(cu_empty, cu);
  TestEncodeDecode(kEncodedCellUnionFace1, cu_face1, &cu);
  EXPECT_EQ(cu_face1, cu);
  TestEncodeDecode(kEncodedCellUnionFromCells, cu_latlngs, &cu);
  EXPECT_EQ(cu_latlngs, cu);
}

TEST_F(S2RegionEncodeDecodeTest, S2LatLngRect) {
  S2LatLngRect rect;
  S2LatLngRect rect_empty = S2LatLngRect::Empty();
  S2LatLngRect rect_full = S2LatLngRect::Full();
  S2LatLngRect rect_from_center_size = S2LatLngRect::FromCenterSize(
      S2LatLng::FromDegrees(80, 170), S2LatLng::FromDegrees(40, 60));

  TestEncodeDecode(kEncodedRectEmpty, rect_empty, &rect);
  EXPECT_TRUE(rect_empty.ApproxEquals(rect));
  TestEncodeDecode(kEncodedRectFull, rect_full, &rect);
  EXPECT_TRUE(rect_full.ApproxEquals(rect));
  TestEncodeDecode(kEncodedRectCentersize, rect_from_center_size, &rect);
  EXPECT_TRUE(rect_from_center_size.ApproxEquals(rect));
}

TEST_F(S2RegionEncodeDecodeTest, S2Loop) {
  const string kCross1 = "-2:1, -1:1, 1:1, 2:1, 2:-1, 1:-1, -1:-1, -2:-1";
  const string kCrossCenterHole = "-0.5:0.5, 0.5:0.5, 0.5:-0.5, -0.5:-0.5;";

  S2Loop loop;
  S2Loop loop_empty(S2Loop::kEmpty());
  S2Loop loop_full(S2Loop::kFull());
  unique_ptr<S2Loop> loop_cross = s2textformat::MakeLoop(kCross1);

  TestEncodeDecode(kEncodedLoopEmpty, loop_empty, &loop);
  EXPECT_TRUE(loop_empty.Equals(&loop));
  TestEncodeDecode(kEncodedLoopFull, loop_full, &loop);
  EXPECT_TRUE(loop_full.Equals(&loop));
  TestEncodeDecode(kEncodedLoopCross, *loop_cross, &loop);
  EXPECT_TRUE(loop_cross->Equals(&loop));
}


TEST_F(S2RegionEncodeDecodeTest, S2PointRegion) {
  S2PointRegion point(S2::Origin());
  S2PointRegion point_origin(S2::Origin());
  S2PointRegion point_testing(S2Point(12.34, 56.78, 9.1011).Normalize());

  TestEncodeDecode(kEncodedPointOrigin, point_origin, &point);
  TestEncodeDecode(kEncodedPointTesting, point_testing, &point);
}

TEST_F(S2RegionEncodeDecodeTest, S2Polygon) {
  const string kCross1 = "-2:1, -1:1, 1:1, 2:1, 2:-1, 1:-1, -1:-1, -2:-1";
  const string kCrossCenterHole = "-0.5:0.5, 0.5:0.5, 0.5:-0.5, -0.5:-0.5;";

  S2Polygon polygon;
  unique_ptr<S2Polygon> polygon_empty = s2textformat::MakePolygon("");
  unique_ptr<S2Polygon> polygon_full =
      s2textformat::MakeVerbatimPolygon("full");
  unique_ptr<S2Polygon> polygon_cross = s2textformat::MakePolygon(kCross1);
  unique_ptr<S2Polygon> polygon_cross_hole =
      s2textformat::MakePolygon(kCross1 + ";" + kCrossCenterHole);

  TestEncodeDecode(kEncodedPolygonEmpty, *polygon_empty, &polygon);
  EXPECT_TRUE(polygon_empty->Equals(&polygon));
  TestEncodeDecode(kEncodedPolygonFull, *polygon_full, &polygon);
  EXPECT_TRUE(polygon_full->Equals(&polygon));
  TestEncodeDecode(kEncodedPolygon1Loops, *polygon_cross, &polygon);
  EXPECT_TRUE(polygon_cross->Equals(&polygon));
  TestEncodeDecode(kEncodedPolygon2Loops, *polygon_cross_hole, &polygon);
  EXPECT_TRUE(polygon_cross_hole->Equals(&polygon));
}

TEST_F(S2RegionEncodeDecodeTest, S2Polyline) {
  S2Polyline polyline;
  vector<S2Point> vertices;
  S2Polyline polyline_empty(vertices);
  vector<S2LatLng> latlngs = {S2LatLng::FromDegrees(0, 0),
                              S2LatLng::FromDegrees(0, 90),
                              S2LatLng::FromDegrees(0, 180)};
  S2Polyline polyline_semi(latlngs);
  unique_ptr<S2Polyline> polyline_3segments =
      s2textformat::MakePolyline("0:0, 0:10, 10:20, 20:30");

  TestEncodeDecode(kEncodedPolylineEmpty, polyline_empty, &polyline);
  EXPECT_TRUE(polyline_empty.Equals(&polyline));
  TestEncodeDecode(kEncodedPolylineSemiEquator, polyline_semi, &polyline);
  EXPECT_TRUE(polyline_semi.Equals(&polyline));
  TestEncodeDecode(kEncodedPolyline3Segments, *polyline_3segments, &polyline);
  EXPECT_TRUE(polyline_3segments->Equals(&polyline));
}

// TODO(user): When the remaining types implement Encode/Decode, add their
// test cases here. i.e. S2R2Rect, S2RegionIntersection, S2RegionUnion, etc.

}  // namespace
