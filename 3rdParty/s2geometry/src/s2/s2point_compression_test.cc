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

#include <cstddef>
#include <string>
#include <vector>

#include "s2/base/commandlineflags.h"
#include "s2/base/logging.h"
#include <gtest/gtest.h>

#include "s2/s1angle.h"
#include "s2/s2cell_id.h"
#include "s2/s2coords.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include "s2/third_party/absl/container/fixed_array.h"
#include "s2/third_party/absl/types/span.h"
#include "s2/util/coding/coder.h"

using absl::FixedArray;
using absl::MakeSpan;
using absl::Span;
using std::vector;

DEFINE_int32(s2point_compression_bm_level, 30,
             "Level to encode at for benchmarks.");
DEFINE_double(s2point_compression_bm_radius_km, 1000.0,
              "Radius to use for loop for benchmarks.");

namespace {

S2Point SnapPointToLevel(const S2Point& point, int level) {
  return S2CellId(point).parent(level).ToPoint();
}

vector<S2Point> SnapPointsToLevel(const vector<S2Point>& points,
                                  int level) {
  vector<S2Point> snapped_points(points.size());
  for (int i = 0; i < points.size(); ++i) {
    snapped_points[i] = SnapPointToLevel(points[i], level);
  }
  return snapped_points;
}

// Make a regular loop around the corner of faces 0, 1, and 2 with the
// specified radius in meters (on the earth) and number of vertices.
vector<S2Point> MakeRegularPoints(int num_vertices,
                                  double radius_km,
                                  int level) {
  const S2Point center = S2Point(1.0, 1.0, 1.0).Normalize();
  const S1Angle radius_angle = S2Testing::KmToAngle(radius_km);

  const vector<S2Point> unsnapped_points =
      S2Testing::MakeRegularPoints(center, radius_angle, num_vertices);

  return SnapPointsToLevel(unsnapped_points, level);
}

void MakeXYZFaceSiTiPoints(Span<const S2Point> points,
                           Span<S2XYZFaceSiTi> result) {
  S2_CHECK_EQ(points.size(), result.size());
  for (int i = 0; i < points.size(); ++i) {
    result[i].xyz = points[i];
    result[i].cell_level = S2::XYZtoFaceSiTi(points[i], &result[i].face,
                                             &result[i].si, &result[i].ti);
  }
}

class S2PointCompressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_4_ = MakeRegularPoints(4, 0.1, S2::kMaxCellLevel);

    const S2Point center = S2Point(1.0, 1.0, 1.0).Normalize();
    const S1Angle radius = S2Testing::KmToAngle(0.1);
    loop_4_unsnapped_ = S2Testing::MakeRegularPoints(center, radius, 4);

    // Radius is 100m, so points are about 141 meters apart.
    // Snapping to level 14 will move them by < 47m.
    loop_4_level_14_ = MakeRegularPoints(4, 0.1, 14);

    loop_100_ = MakeRegularPoints(100, 0.1, S2::kMaxCellLevel);

    loop_100_unsnapped_ = S2Testing::MakeRegularPoints(center, radius, 100);

    loop_100_mixed_15_ = S2Testing::MakeRegularPoints(center, radius, 100);
    for (int i = 0; i < 15; ++i) {
      loop_100_mixed_15_[3 * i] = SnapPointToLevel(loop_100_mixed_15_[3 * i],
                                                   S2::kMaxCellLevel);
    }

    loop_100_mixed_25_ = S2Testing::MakeRegularPoints(center, radius, 100);
    for (int i = 0; i < 25; ++i) {
      loop_100_mixed_25_[4 * i] = SnapPointToLevel(loop_100_mixed_25_[4 * i],
                                                   S2::kMaxCellLevel);
    }

    // Circumference is 628m, so points are about 6 meters apart.
    // Snapping to level 22 will move them by < 2m.
    loop_100_level_22_ = MakeRegularPoints(100, 0.1, 22);

    vector<S2Point> multi_face_points(6);
    multi_face_points[0] = S2::FaceUVtoXYZ(0, -0.5, 0.5).Normalize();
    multi_face_points[1] = S2::FaceUVtoXYZ(1, -0.5, 0.5).Normalize();
    multi_face_points[2] = S2::FaceUVtoXYZ(1, 0.5, -0.5).Normalize();
    multi_face_points[3] = S2::FaceUVtoXYZ(2, -0.5, 0.5).Normalize();
    multi_face_points[4] = S2::FaceUVtoXYZ(2, 0.5, -0.5).Normalize();
    multi_face_points[5] = S2::FaceUVtoXYZ(2, 0.5, 0.5).Normalize();
    loop_multi_face_ = SnapPointsToLevel(multi_face_points, S2::kMaxCellLevel);

    vector<S2Point> line_points(100);
    for (int i = 0; i < line_points.size(); ++i) {
      const double s = 0.01 + 0.005 * i;
      const double t = 0.01 + 0.009 * i;
      const double u = S2::STtoUV(s);
      const double v = S2::STtoUV(t);
      line_points[i] = S2::FaceUVtoXYZ(0, u, v).Normalize();
    }
    line_ = SnapPointsToLevel(line_points, S2::kMaxCellLevel);
  }

  void Encode(Span<const S2Point> points, int level) {
    FixedArray<S2XYZFaceSiTi> pts(points.size());
    MakeXYZFaceSiTiPoints(points, MakeSpan(pts));
    S2EncodePointsCompressed(pts, level, &encoder_);
  }

  void Decode(int level, Span<S2Point> points) {
    decoder_.reset(encoder_.base(), encoder_.length());
    EXPECT_TRUE(S2DecodePointsCompressed(&decoder_, level, points));
  }

  void Roundtrip(const vector<S2Point>& loop, int level) {
    Encode(loop, level);
    vector<S2Point> points(loop.size());
    Decode(level, MakeSpan(points));

    EXPECT_TRUE(loop == points)
        << "Decoded points\n" << s2textformat::ToString(points)
        << "\ndo not match original points\n"
        << s2textformat::ToString(loop);
  }

  Encoder encoder_;
  Decoder decoder_;

  // Four vertex loop near the corner of faces 0, 1, and 2.
  vector<S2Point> loop_4_;
  // Four vertex loop near the corner of faces 0, 1, and 2;
  // unsnapped.
  vector<S2Point> loop_4_unsnapped_;
  // Four vertex loop near the corner of faces 0, 1, and 2;
  // snapped to level 14.
  vector<S2Point> loop_4_level_14_;
  // 100 vertex loop near the corner of faces 0, 1, and 2.
  vector<S2Point> loop_100_;
  // 100 vertex loop near the corner of faces 0, 1, and 2;
  // unsnapped.
  vector<S2Point> loop_100_unsnapped_;
  // 100 vertex loop near the corner of faces 0, 1, and 2;
  // 15 points snapped to kMakCellLevel, the others not snapped.
  vector<S2Point> loop_100_mixed_15_;
  // 100 vertex loop near the corner of faces 0, 1, and 2;
  // 25 points snapped to kMakCellLevel, the others not snapped.
  vector<S2Point> loop_100_mixed_25_;
  // 100 vertex loop near the corner of faces 0, 1, and 2;
  // snapped to level 22.
  vector<S2Point> loop_100_level_22_;
  // A loop with two vertices on each of three faces.
  vector<S2Point> loop_multi_face_;
  // A straight line of 100 vertices on face 0 that should compress well.
  vector<S2Point> line_;
};

TEST_F(S2PointCompressionTest, RoundtripsEmpty) {
  // Just check this doesn't crash.
  Encode(Span<S2Point>(), S2::kMaxCellLevel);
  Decode(S2::kMaxCellLevel, Span<S2Point>());
}

TEST_F(S2PointCompressionTest, RoundtripsFourVertexLoop) {
  Roundtrip(loop_4_, S2::kMaxCellLevel);
}

TEST_F(S2PointCompressionTest, RoundtripsFourVertexLoopUnsnapped) {
  Roundtrip(loop_4_unsnapped_, S2::kMaxCellLevel);
}

TEST_F(S2PointCompressionTest, FourVertexLoopSize) {
  Encode(loop_4_, S2::kMaxCellLevel);
  // It would take 32 bytes uncompressed.
  EXPECT_EQ(39, encoder_.length());
}

TEST_F(S2PointCompressionTest, RoundtripsFourVertexLevel14Loop) {
  const int level = 14;
  Roundtrip(loop_4_level_14_, level);
}

TEST_F(S2PointCompressionTest, FourVertexLevel14LoopSize) {
  const int level = 14;
  Encode(loop_4_level_14_, level);
  // It would take 4 bytes per vertex without compression.
  EXPECT_EQ(23, encoder_.length());
}

TEST_F(S2PointCompressionTest, Roundtrips100VertexLoop) {
  Roundtrip(loop_100_, S2::kMaxCellLevel);
}

TEST_F(S2PointCompressionTest, Roundtrips100VertexLoopUnsnapped) {
  Roundtrip(loop_100_unsnapped_, S2::kMaxCellLevel);
}

TEST_F(S2PointCompressionTest, Roundtrips100VertexLoopMixed15) {
  Roundtrip(loop_100_mixed_15_, S2::kMaxCellLevel);
  EXPECT_EQ(2381, encoder_.length());
}

TEST_F(S2PointCompressionTest, Roundtrips100VertexLoopMixed25) {
  Roundtrip(loop_100_mixed_25_, S2::kMaxCellLevel);
  EXPECT_EQ(2131, encoder_.length());
}

TEST_F(S2PointCompressionTest, OneHundredVertexLoopSize) {
  Encode(loop_100_, S2::kMaxCellLevel);
  EXPECT_EQ(257, encoder_.length());
}

TEST_F(S2PointCompressionTest, OneHundredVertexLoopUnsnappedSize) {
  Encode(loop_100_unsnapped_, S2::kMaxCellLevel);
  EXPECT_EQ(2756, encoder_.length());
}

TEST_F(S2PointCompressionTest, Roundtrips100VertexLevel22Loop) {
  const int level = 22;
  Roundtrip(loop_100_level_22_, level);
}

TEST_F(S2PointCompressionTest, OneHundredVertexLoopLevel22Size) {
  Encode(loop_100_level_22_, 22);
  EXPECT_EQ(148, encoder_.length());
}

TEST_F(S2PointCompressionTest, MultiFaceLoop) {
  Roundtrip(loop_multi_face_, S2::kMaxCellLevel);
}

TEST_F(S2PointCompressionTest, StraightLineCompressesWell) {
  Roundtrip(line_, S2::kMaxCellLevel);
  // About 1 byte / vertex.
  EXPECT_EQ(line_.size() + 17, encoder_.length());
}

TEST_F(S2PointCompressionTest, FirstPointOnFaceEdge) {
  // This test used to trigger a bug in which EncodeFirstPointFixedLength()
  // tried to encode a pi/qi value of (2**level) in "level" bits (which did
  // not work out so well).  The fix is documented in SiTitoPiQi().
  //
  // The test data consists of two points, where the first point is exactly on
  // an S2Cell face edge (with ti == S2::kMaxSiTi), and the second point is
  // encodable at snap level 8.  This used to cause the code to try encoding
  // qi = 256 in 8 bits.
  S2XYZFaceSiTi points[] = {
    {
      S2Point(0.054299323861222645, -0.70606358900180299,
              0.70606358900180299),
      2,                      // face
      956301312, 2147483648,  // si, ti
      -1                      // level
    },
    {
      S2Point(0.056482651436986935, -0.70781701406865505,
              0.70413406726388494),
      4,                    // face
      4194304, 1195376640,  // si, ti
      8                     // level
    }};

  Encoder encoder;
  S2EncodePointsCompressed(points, 8, &encoder);
  Decoder decoder(encoder.base(), encoder.length());
  S2Point result[2];
  S2DecodePointsCompressed(&decoder, 8, result);
  S2_CHECK(result[0] == points[0].xyz);
  S2_CHECK(result[1] == points[1].xyz);
}

}  // namespace
