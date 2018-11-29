// Copyright 2005 Google Inc. All Rights Reserved.
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

#include "s2/s2cell_id.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iosfwd>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "s2/base/logging.h"
#include "s2/r2.h"
#include "s2/r2rect.h"
#include "s2/s2cap.h"
#include "s2/s2coords.h"
#include "s2/s2latlng.h"
#include "s2/s2metrics.h"
#include "s2/s2testing.h"
#include "s2/third_party/absl/base/macros.h"

using S2::internal::kPosToOrientation;
using std::fabs;
using std::min;
using std::unordered_map;
using std::vector;

static S2CellId GetCellId(double lat_degrees, double lng_degrees) {
  S2CellId id(S2LatLng::FromDegrees(lat_degrees, lng_degrees));
  S2_LOG(INFO) << std::hex << id.id();
  return id;
}

TEST(S2CellId, DefaultConstructor) {
  S2CellId id;
  EXPECT_EQ(0, id.id());
  EXPECT_FALSE(id.is_valid());
}

TEST(S2CellId, S2CellIdHash) {
  EXPECT_EQ(S2CellIdHash()(GetCellId(0, 90)),
            S2CellIdHash()(GetCellId(0, 90)));
}

TEST(S2CellId, FaceDefinitions) {
  EXPECT_EQ(0, GetCellId(0, 0).face());
  EXPECT_EQ(1, GetCellId(0, 90).face());
  EXPECT_EQ(2, GetCellId(90, 0).face());
  EXPECT_EQ(3, GetCellId(0, 180).face());
  EXPECT_EQ(4, GetCellId(0, -90).face());
  EXPECT_EQ(5, GetCellId(-90, 0).face());
}

TEST(S2CellId, FromFace) {
  for (int face = 0; face < 6; ++face) {
    EXPECT_EQ(S2CellId::FromFacePosLevel(face, 0, 0), S2CellId::FromFace(face));
  }
}

TEST(S2CellId, ParentChildRelationships) {
  S2CellId id = S2CellId::FromFacePosLevel(3, 0x12345678,
                                           S2CellId::kMaxLevel - 4);
  EXPECT_TRUE(id.is_valid());
  EXPECT_EQ(3, id.face());
  EXPECT_EQ(0x12345700, id.pos());
  EXPECT_EQ(S2CellId::kMaxLevel - 4, id.level());
  EXPECT_FALSE(id.is_leaf());

  EXPECT_EQ(0x12345610, id.child_begin(id.level() + 2).pos());
  EXPECT_EQ(0x12345640, id.child_begin().pos());
  EXPECT_EQ(0x12345400, id.parent().pos());
  EXPECT_EQ(0x12345000, id.parent(id.level() - 2).pos());

  // Check ordering of children relative to parents.
  EXPECT_LT(id.child_begin(), id);
  EXPECT_GT(id.child_end(), id);
  EXPECT_EQ(id.child_end(), id.child_begin().next().next().next().next());
  EXPECT_EQ(id.range_min(), id.child_begin(S2CellId::kMaxLevel));
  EXPECT_EQ(id.range_max().next(), id.child_end(S2CellId::kMaxLevel));

  // Check that cells are represented by the position of their center
  // along the Hilbert curve.
  EXPECT_EQ(2 * id.id(), id.range_min().id() + id.range_max().id());
}

TEST(S2CellId, SentinelRangeMinMax) {
  EXPECT_EQ(S2CellId::Sentinel(), S2CellId::Sentinel().range_min());
  EXPECT_EQ(S2CellId::Sentinel(), S2CellId::Sentinel().range_max());
}

TEST(S2CellId, CenterSiTi) {
  S2CellId id = S2CellId::FromFacePosLevel(3, 0x12345678,
                                           S2CellId::kMaxLevel);
  // Check that the (si, ti) coordinates of the center end in a
  // 1 followed by (30 - level) 0s.
  int si, ti;

  // Leaf level, 30.
  id.GetCenterSiTi(&si, &ti);
  S2_CHECK_EQ(1 << 0, si & 1);
  S2_CHECK_EQ(1 << 0, ti & 1);

  // Level 29.
  id.parent(S2CellId::kMaxLevel - 1).GetCenterSiTi(&si, &ti);
  S2_CHECK_EQ(1 << 1, si & 3);
  S2_CHECK_EQ(1 << 1, ti & 3);

  // Level 28.
  id.parent(S2CellId::kMaxLevel - 2).GetCenterSiTi(&si, &ti);
  S2_CHECK_EQ(1 << 2, si & 7);
  S2_CHECK_EQ(1 << 2, ti & 7);

  // Level 20.
  id.parent(S2CellId::kMaxLevel - 10).GetCenterSiTi(&si, &ti);
  S2_CHECK_EQ(1 << 10, si & ((1 << 11) - 1));
  S2_CHECK_EQ(1 << 10, ti & ((1 << 11) - 1));

  // Level 10.
  id.parent(S2CellId::kMaxLevel - 20).GetCenterSiTi(&si, &ti);
  S2_CHECK_EQ(1 << 20, si & ((1 << 21) - 1));
  S2_CHECK_EQ(1 << 20, ti & ((1 << 21) - 1));

  // Level 0.
  id.parent(0).GetCenterSiTi(&si, &ti);
  S2_CHECK_EQ(1 << 30, si & ((1U << 31) - 1));
  S2_CHECK_EQ(1 << 30, ti & ((1U << 31) - 1));
}

TEST(S2CellId, Wrapping) {
  // Check wrapping from beginning of Hilbert curve to end and vice versa.
  EXPECT_EQ(S2CellId::End(0).prev(), S2CellId::Begin(0).prev_wrap());

  EXPECT_EQ(S2CellId::FromFacePosLevel(
                5, ~0ULL >> S2CellId::kFaceBits, S2CellId::kMaxLevel),
            S2CellId::Begin(S2CellId::kMaxLevel).prev_wrap());
  EXPECT_EQ(S2CellId::FromFacePosLevel(
                5, ~0ULL >> S2CellId::kFaceBits, S2CellId::kMaxLevel),
            S2CellId::Begin(S2CellId::kMaxLevel).advance_wrap(-1));

  EXPECT_EQ(S2CellId::Begin(4), S2CellId::End(4).prev().next_wrap());
  EXPECT_EQ(S2CellId::Begin(4), S2CellId::End(4).advance(-1).advance_wrap(1));

  EXPECT_EQ(S2CellId::FromFacePosLevel(0, 0, S2CellId::kMaxLevel),
            S2CellId::End(S2CellId::kMaxLevel).prev().next_wrap());
  EXPECT_EQ(S2CellId::FromFacePosLevel(0, 0, S2CellId::kMaxLevel),
            S2CellId::End(S2CellId::kMaxLevel).advance(-1).advance_wrap(1));
}

TEST(S2CellId, Advance) {
  S2CellId id = S2CellId::FromFacePosLevel(3, 0x12345678,
                                           S2CellId::kMaxLevel - 4);
  // Check basic properties of advance().
  EXPECT_EQ(S2CellId::End(0), S2CellId::Begin(0).advance(7));
  EXPECT_EQ(S2CellId::End(0), S2CellId::Begin(0).advance(12));
  EXPECT_EQ(S2CellId::Begin(0), S2CellId::End(0).advance(-7));
  EXPECT_EQ(S2CellId::Begin(0), S2CellId::End(0).advance(-12000000));
  int num_level_5_cells = 6 << (2 * 5);
  EXPECT_EQ(S2CellId::End(5).advance(500 - num_level_5_cells),
            S2CellId::Begin(5).advance(500));
  EXPECT_EQ(id.next().child_begin(S2CellId::kMaxLevel),
            id.child_begin(S2CellId::kMaxLevel).advance(256));
  EXPECT_EQ(S2CellId::FromFacePosLevel(5, 0, S2CellId::kMaxLevel),
            S2CellId::FromFacePosLevel(1, 0, S2CellId::kMaxLevel)
            .advance(4ULL << (2 * S2CellId::kMaxLevel)));

  // Check basic properties of advance_wrap().
  EXPECT_EQ(S2CellId::FromFace(1), S2CellId::Begin(0).advance_wrap(7));
  EXPECT_EQ(S2CellId::Begin(0), S2CellId::Begin(0).advance_wrap(12));
  EXPECT_EQ(S2CellId::FromFace(4), S2CellId::FromFace(5).advance_wrap(-7));
  EXPECT_EQ(S2CellId::Begin(0), S2CellId::Begin(0).advance_wrap(-12000000));
  EXPECT_EQ(S2CellId::Begin(5).advance_wrap(6644),
            S2CellId::Begin(5).advance_wrap(-11788));
  EXPECT_EQ(id.next().child_begin(S2CellId::kMaxLevel),
            id.child_begin(S2CellId::kMaxLevel).advance_wrap(256));
  EXPECT_EQ(S2CellId::FromFacePosLevel(1, 0, S2CellId::kMaxLevel),
            S2CellId::FromFacePosLevel(5, 0, S2CellId::kMaxLevel)
            .advance_wrap(2ULL << (2 * S2CellId::kMaxLevel)));
}

TEST(S2CellId, DistanceFromBegin) {
  EXPECT_EQ(6, S2CellId::End(0).distance_from_begin());
  EXPECT_EQ(6 * (1LL << (2 * S2CellId::kMaxLevel)),
            S2CellId::End(S2CellId::kMaxLevel).distance_from_begin());

  EXPECT_EQ(0, S2CellId::Begin(0).distance_from_begin());
  EXPECT_EQ(0, S2CellId::Begin(S2CellId::kMaxLevel).distance_from_begin());

  S2CellId id = S2CellId::FromFacePosLevel(3, 0x12345678,
                                           S2CellId::kMaxLevel - 4);
  EXPECT_EQ(id, S2CellId::Begin(id.level()).advance(id.distance_from_begin()));
}

TEST(S2CellId, MaximumTile) {
  // This method is tested more thoroughly in s2cell_union_test.cc.
  for (int iter = 0; iter < 1000; ++iter) {
    S2CellId id = S2Testing::GetRandomCellId(10);

    // Check that "limit" is returned for tiles at or beyond "limit".
    EXPECT_EQ(id, id.maximum_tile(id));
    EXPECT_EQ(id, id.child(0).maximum_tile(id));
    EXPECT_EQ(id, id.child(1).maximum_tile(id));
    EXPECT_EQ(id, id.next().maximum_tile(id));
    EXPECT_EQ(id.child(0), id.maximum_tile(id.child(0)));

    // Check that the tile size is increased when possible.
    EXPECT_EQ(id, id.child(0).maximum_tile(id.next()));
    EXPECT_EQ(id, id.child(0).maximum_tile(id.next().child(0)));
    EXPECT_EQ(id, id.child(0).maximum_tile(id.next().child(1).child(0)));
    EXPECT_EQ(id, id.child(0).child(0).maximum_tile(id.next()));
    EXPECT_EQ(id, id.child(0).child(0).child(0).maximum_tile(id.next()));

    // Check that the tile size is decreased when necessary.
    EXPECT_EQ(id.child(0), id.maximum_tile(id.child(0).next()));
    EXPECT_EQ(id.child(0), id.maximum_tile(id.child(0).next().child(0)));
    EXPECT_EQ(id.child(0), id.maximum_tile(id.child(0).next().child(1)));
    EXPECT_EQ(id.child(0).child(0),
              id.maximum_tile(id.child(0).child(0).next()));
    EXPECT_EQ(id.child(0).child(0).child(0),
              id.maximum_tile(id.child(0).child(0).child(0).next()));

    // Check that the tile size is otherwise unchanged.
    EXPECT_EQ(id, id.maximum_tile(id.next()));
    EXPECT_EQ(id, id.maximum_tile(id.next().child(0)));
    EXPECT_EQ(id, id.maximum_tile(id.next().child(1).child(0)));
  }
}

TEST(S2CellId, GetCommonAncestorLevel) {
  // Two identical cell ids.
  EXPECT_EQ(0, S2CellId::FromFace(0).
            GetCommonAncestorLevel(S2CellId::FromFace(0)));
  EXPECT_EQ(30, S2CellId::FromFace(0).child_begin(30).
            GetCommonAncestorLevel(S2CellId::FromFace(0).child_begin(30)));

  // One cell id is a descendant of the other.
  EXPECT_EQ(0, S2CellId::FromFace(0).child_begin(30).
            GetCommonAncestorLevel(S2CellId::FromFace(0)));
  EXPECT_EQ(0, S2CellId::FromFace(5).
            GetCommonAncestorLevel(S2CellId::FromFace(5).child_end(30).prev()));

  // Two cells that have no common ancestor.
  EXPECT_EQ(-1, S2CellId::FromFace(0).
            GetCommonAncestorLevel(S2CellId::FromFace(5)));
  EXPECT_EQ(-1, S2CellId::FromFace(2).child_begin(30).
            GetCommonAncestorLevel(S2CellId::FromFace(3).child_end(20)));

  // Two cells that have a common ancestor distinct from both of them.
  EXPECT_EQ(8, S2CellId::FromFace(5).child_begin(9).next().child_begin(15).
            GetCommonAncestorLevel(
                S2CellId::FromFace(5).child_begin(9).child_begin(20)));
  EXPECT_EQ(1, S2CellId::FromFace(0).child_begin(2).child_begin(30).
            GetCommonAncestorLevel(
                S2CellId::FromFace(0).child_begin(2).next().child_begin(5)));
}

TEST(S2CellId, Inverses) {
  // Check the conversion of random leaf cells to S2LatLngs and back.
  for (int i = 0; i < 200000; ++i) {
    S2CellId id = S2Testing::GetRandomCellId(S2CellId::kMaxLevel);
    EXPECT_TRUE(id.is_leaf());
    EXPECT_EQ(S2CellId::kMaxLevel, id.level());
    S2LatLng center = id.ToLatLng();
    EXPECT_EQ(id.id(), S2CellId(center).id());
  }
}

TEST(S2CellId, Tokens) {
  // Test random cell ids at all levels.
  for (int i = 0; i < 10000; ++i) {
    S2CellId id = S2Testing::GetRandomCellId();
    string token = id.ToToken();
    EXPECT_LE(token.size(), 16);
    EXPECT_EQ(id, S2CellId::FromToken(token));
    EXPECT_EQ(id, S2CellId::FromToken(token.data(), token.size()));
  }
  // Check that invalid cell ids can be encoded, and round-trip is
  // the identity operation.
  string token = S2CellId::None().ToToken();
  EXPECT_EQ(S2CellId::None(), S2CellId::FromToken(token));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromToken(token.data(), token.size()));

  // Sentinel is invalid.
  token = S2CellId::Sentinel().ToToken();
  EXPECT_EQ(S2CellId::FromToken(token), S2CellId::Sentinel());
  EXPECT_EQ(S2CellId::FromToken(token.data(), token.size()),
            S2CellId::Sentinel());

  // Check an invalid face.
  token = S2CellId::FromFace(7).ToToken();
  EXPECT_EQ(S2CellId::FromToken(token), S2CellId::FromFace(7));
  EXPECT_EQ(S2CellId::FromToken(token.data(), token.size()),
            S2CellId::FromFace(7));

  // Check that supplying tokens with non-alphanumeric characters
  // returns S2CellId::None().
  EXPECT_EQ(S2CellId::None(), S2CellId::FromToken("876b e99"));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromToken("876bee99\n"));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromToken("876[ee99"));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromToken(" 876bee99"));
}

TEST(S2CellId, EncodeDecode) {
  S2CellId id(0x7837423);
  Encoder encoder;
  id.Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  S2CellId decoded_id;
  EXPECT_TRUE(decoded_id.Decode(&decoder));
  EXPECT_EQ(id, decoded_id);
}

TEST(S2CellId, EncodeDecodeNoneCell) {
  S2CellId none_id = S2CellId::None();
  Encoder encoder;
  none_id.Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  S2CellId decoded_id;
  EXPECT_TRUE(decoded_id.Decode(&decoder));
  EXPECT_EQ(none_id, decoded_id);
}

TEST(S2CellId, DecodeFailsWithTruncatedBuffer) {
  S2CellId id(0x7837423);
  Encoder encoder;
  id.Encode(&encoder);

  // Truncate encoded buffer.
  Decoder decoder(encoder.base(), encoder.length() - 2);
  S2CellId decoded_id;
  EXPECT_FALSE(decoded_id.Decode(&decoder));
}


static const int kMaxExpandLevel = 3;

static void ExpandCell(
    S2CellId parent, vector<S2CellId>* cells,
    unordered_map<S2CellId, S2CellId, S2CellIdHash>* parent_map) {
  cells->push_back(parent);
  if (parent.level() == kMaxExpandLevel) return;
  int i, j, orientation;
  int face = parent.ToFaceIJOrientation(&i, &j, &orientation);
  EXPECT_EQ(parent.face(), face);

  S2CellId child = parent.child_begin();
  for (int pos = 0; child != parent.child_end(); child = child.next(), ++pos) {
    (*parent_map)[child] = parent;
    // Do some basic checks on the children.
    EXPECT_EQ(child, parent.child(pos));
    EXPECT_EQ(pos, child.child_position());
    // Test child_position(level) on all the child's ancestors.
    for (S2CellId ancestor = child; ancestor.level() >= 1;
         ancestor = (*parent_map)[ancestor]) {
      EXPECT_EQ(child.child_position(ancestor.level()),
                ancestor.child_position());
    }
    EXPECT_EQ(pos, child.child_position(child.level()));
    EXPECT_EQ(parent.level() + 1, child.level());
    EXPECT_FALSE(child.is_leaf());
    int child_orientation;
    EXPECT_EQ(face, child.ToFaceIJOrientation(&i, &j, &child_orientation));
    EXPECT_EQ(orientation ^ kPosToOrientation[pos], child_orientation);
    ExpandCell(child, cells, parent_map);
  }
}

TEST(S2CellId, Containment) {
  // Test contains() and intersects().
  unordered_map<S2CellId, S2CellId, S2CellIdHash> parent_map;
  vector<S2CellId> cells;
  for (int face = 0; face < 6; ++face) {
    ExpandCell(S2CellId::FromFace(face), &cells, &parent_map);
  }
  for (S2CellId end_id : cells) {
    for (S2CellId begin_id : cells) {
      bool contained = true;
      for (S2CellId id = begin_id; id != end_id; id = parent_map[id]) {
        if (parent_map.find(id) == parent_map.end()) {
          contained = false;
          break;
        }
      }
      EXPECT_EQ(contained, end_id.contains(begin_id));
      EXPECT_EQ(contained,
                begin_id >= end_id.range_min() &&
                begin_id <= end_id.range_max());
      EXPECT_EQ(end_id.intersects(begin_id),
                end_id.contains(begin_id) || begin_id.contains(end_id));
    }
  }
}

static const int kMaxWalkLevel = 8;

TEST(S2CellId, Continuity) {
  // Make sure that sequentially increasing cell ids form a continuous
  // path over the surface of the sphere, i.e. there are no
  // discontinuous jumps from one region to another.

  double max_dist = S2::kMaxEdge.GetValue(kMaxWalkLevel);
  S2CellId end = S2CellId::End(kMaxWalkLevel);
  S2CellId id = S2CellId::Begin(kMaxWalkLevel);
  for (; id != end; id = id.next()) {
    EXPECT_LE(id.ToPointRaw().Angle(id.next_wrap().ToPointRaw()), max_dist);
    EXPECT_EQ(id.next_wrap(), id.advance_wrap(1));
    EXPECT_EQ(id, id.next_wrap().advance_wrap(-1));

    // Check that the ToPointRaw() returns the center of each cell
    // in (s,t) coordinates.
    double u, v;
    S2::XYZtoFaceUV(id.ToPointRaw(), &u, &v);
    static const double kCellSize = 1.0 / (1 << kMaxWalkLevel);
    EXPECT_NEAR(remainder(S2::UVtoST(u), 0.5 * kCellSize), 0.0, 1e-15);
    EXPECT_NEAR(remainder(S2::UVtoST(v), 0.5 * kCellSize), 0.0, 1e-15);
  }
}

TEST(S2CellId, Coverage) {
  // Make sure that random points on the sphere can be represented to the
  // expected level of accuracy, which in the worst case is sqrt(2/3) times
  // the maximum arc length between the points on the sphere associated with
  // adjacent values of "i" or "j".  (It is sqrt(2/3) rather than 1/2 because
  // the cells at the corners of each face are stretched -- they have 60 and
  // 120 degree angles.)

  double max_dist = 0.5 * S2::kMaxDiag.GetValue(S2CellId::kMaxLevel);
  for (int i = 0; i < 1000000; ++i) {
    S2Point p = S2Testing::RandomPoint();
    S2Point q = S2CellId(p).ToPointRaw();
    EXPECT_LE(p.Angle(q), max_dist);
  }
}

static void TestAllNeighbors(S2CellId id, int level) {
  S2_DCHECK_GE(level, id.level());
  S2_DCHECK_LT(level, S2CellId::kMaxLevel);

  // We compute AppendAllNeighbors, and then add in all the children of "id"
  // at the given level.  We then compare this against the result of finding
  // all the vertex neighbors of all the vertices of children of "id" at the
  // given level.  These should give the same result.
  vector<S2CellId> all, expected;
  id.AppendAllNeighbors(level, &all);
  S2CellId end = id.child_end(level + 1);
  for (S2CellId c = id.child_begin(level + 1); c != end; c = c.next()) {
    all.push_back(c.parent());
    c.AppendVertexNeighbors(level, &expected);
  }
  // Sort the results and eliminate duplicates.
  std::sort(all.begin(), all.end());
  std::sort(expected.begin(), expected.end());
  all.erase(std::unique(all.begin(), all.end()), all.end());
  expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
  EXPECT_EQ(expected, all);
}

TEST(S2CellId, Neighbors) {
  // Check the edge neighbors of face 1.
  static int out_faces[] = { 5, 3, 2, 0 };
  S2CellId face_nbrs[4];
  S2CellId::FromFace(1).GetEdgeNeighbors(face_nbrs);
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(face_nbrs[i].is_face());
    EXPECT_EQ(out_faces[i], face_nbrs[i].face());
  }

  // Check the edge neighbors of the corner cells at all levels.  This case is
  // trickier because it requires projecting onto adjacent faces.
  static const int kMaxIJ = S2CellId::kMaxSize - 1;
  for (int level = 1; level <= S2CellId::kMaxLevel; ++level) {
    S2CellId id = S2CellId::FromFaceIJ(1, 0, 0).parent(level);
    S2CellId nbrs[4];
    id.GetEdgeNeighbors(nbrs);
    // These neighbors were determined manually using the face and axis
    // relationships defined in s2coords.cc.
    int size_ij = S2CellId::GetSizeIJ(level);
    EXPECT_EQ(S2CellId::FromFaceIJ(5, kMaxIJ, kMaxIJ).parent(level), nbrs[0]);
    EXPECT_EQ(S2CellId::FromFaceIJ(1, size_ij, 0).parent(level), nbrs[1]);
    EXPECT_EQ(S2CellId::FromFaceIJ(1, 0, size_ij).parent(level), nbrs[2]);
    EXPECT_EQ(S2CellId::FromFaceIJ(0, kMaxIJ, 0).parent(level), nbrs[3]);
  }

  // Check the vertex neighbors of the center of face 2 at level 5.
  vector<S2CellId> nbrs;
  S2CellId(S2Point(0, 0, 1)).AppendVertexNeighbors(5, &nbrs);
  std::sort(nbrs.begin(), nbrs.end());
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(S2CellId::FromFaceIJ(
                 2, (1 << 29) - (i < 2), (1 << 29) - (i == 0 || i == 3))
              .parent(5),
              nbrs[i]);
  }
  nbrs.clear();

  // Check the vertex neighbors of the corner of faces 0, 4, and 5.
  S2CellId id = S2CellId::FromFacePosLevel(0, 0, S2CellId::kMaxLevel);
  id.AppendVertexNeighbors(0, &nbrs);
  std::sort(nbrs.begin(), nbrs.end());
  ASSERT_EQ(3, nbrs.size());
  EXPECT_EQ(S2CellId::FromFace(0), nbrs[0]);
  EXPECT_EQ(S2CellId::FromFace(4), nbrs[1]);
  EXPECT_EQ(S2CellId::FromFace(5), nbrs[2]);

  // Check that AppendAllNeighbors produces results that are consistent
  // with AppendVertexNeighbors for a bunch of random cells.
  for (int i = 0; i < 1000; ++i) {
    S2CellId id = S2Testing::GetRandomCellId();
    if (id.is_leaf()) id = id.parent();

    // TestAllNeighbors computes approximately 2**(2*(diff+1)) cell ids,
    // so it's not reasonable to use large values of "diff".
    int max_diff = min(5, S2CellId::kMaxLevel - id.level() - 1);
    int level = id.level() + S2Testing::rnd.Uniform(max_diff + 1);
    TestAllNeighbors(id, level);
  }
}

// Returns a random point on the boundary of the given rectangle.
static R2Point SampleBoundary(const R2Rect& rect) {
  R2Point uv;
  int d = S2Testing::rnd.Uniform(2);
  uv[d] = S2Testing::rnd.UniformDouble(rect[d][0], rect[d][1]);
  uv[1-d] = S2Testing::rnd.OneIn(2) ? rect[1-d][0] : rect[1-d][1];
  return uv;
}

// Returns the closest point to "uv" on the boundary of "rect".
static R2Point ProjectToBoundary(const R2Point& uv, const R2Rect& rect) {
  double du0 = fabs(uv[0] - rect[0][0]);
  double du1 = fabs(uv[0] - rect[0][1]);
  double dv0 = fabs(uv[1] - rect[1][0]);
  double dv1 = fabs(uv[1] - rect[1][1]);
  double dmin = min(min(du0, du1), min(dv0, dv1));
  if (du0 == dmin) return R2Point(rect[0][0], rect[1].Project(uv[1]));
  if (du1 == dmin) return R2Point(rect[0][1], rect[1].Project(uv[1]));
  if (dv0 == dmin) return R2Point(rect[0].Project(uv[0]), rect[1][0]);
  S2_CHECK_EQ(dmin, dv1) << "Bug in ProjectToBoundary";
  return R2Point(rect[0].Project(uv[0]), rect[1][1]);
}

void TestExpandedByDistanceUV(S2CellId id, S1Angle distance) {
  R2Rect bound = id.GetBoundUV();
  R2Rect expanded = S2CellId::ExpandedByDistanceUV(bound, distance);
  for (int iter = 0; iter < 100; ++iter) {
    // Choose a point on the boundary of the rectangle.
    int face = S2Testing::rnd.Uniform(6);
    R2Point center_uv = SampleBoundary(bound);
    S2Point center = S2::FaceUVtoXYZ(face, center_uv).Normalize();

    // Now sample a point from a disc of radius (2 * distance).
    S2Point p = S2Testing::SamplePoint(S2Cap(center, 2 * distance.abs()));

    // Find the closest point on the boundary to the sampled point.
    R2Point uv;
    if (!S2::FaceXYZtoUV(face, p, &uv)) continue;
    R2Point closest_uv = ProjectToBoundary(uv, bound);
    S2Point closest = S2::FaceUVtoXYZ(face, closest_uv).Normalize();
    S1Angle actual_dist = S1Angle(p, closest);

    if (distance >= S1Angle::Zero()) {
      // "expanded" should contain all points in the original bound, and also
      // all points within "distance" of the boundary.
      if (bound.Contains(uv) || actual_dist < distance) {
        EXPECT_TRUE(expanded.Contains(uv));
      }
    } else {
      // "expanded" should not contain any points within "distance" of the
      // original boundary.
      if (actual_dist < -distance) {
        EXPECT_FALSE(expanded.Contains(uv));
      }
    }
  }
}

TEST(S2CellId, ExpandedByDistanceUV) {
  double max_dist_degrees = 10;
  for (int iter = 0; iter < 100; ++iter) {
    S2CellId id = S2Testing::GetRandomCellId();
    double dist_degrees = S2Testing::rnd.UniformDouble(-max_dist_degrees,
                                                       max_dist_degrees);
    TestExpandedByDistanceUV(id, S1Angle::Degrees(dist_degrees));
  }
}

TEST(S2CellId, ToString) {
  EXPECT_EQ("3/", S2CellId::FromFace(3).ToString());
  EXPECT_EQ("4/000000000000000000000000000000",
            S2CellId::FromFace(4).range_min().ToString());
  EXPECT_EQ("Invalid: 0000000000000000", S2CellId::None().ToString());
}

TEST(S2CellId, FromDebugString) {
  EXPECT_EQ(S2CellId::FromFace(3), S2CellId::FromDebugString("3/"));
  EXPECT_EQ(S2CellId::FromFace(0).child(2).child(1),
            S2CellId::FromDebugString("0/21"));
  EXPECT_EQ(S2CellId::FromFace(4).range_min(),
            S2CellId::FromDebugString("4/000000000000000000000000000000"));
  EXPECT_EQ(S2CellId::None(),
            S2CellId::FromDebugString("4/0000000000000000000000000000000"));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromDebugString(""));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromDebugString("7/"));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromDebugString(" /"));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromDebugString("3:0"));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromDebugString("3/ 12"));
  EXPECT_EQ(S2CellId::None(), S2CellId::FromDebugString("3/1241"));
}

TEST(S2CellId, OutputOperator) {
  S2CellId cell(0xbb04000000000000ULL);
  std::ostringstream s;
  s << cell;
  EXPECT_EQ("5/31200", s.str());
}

