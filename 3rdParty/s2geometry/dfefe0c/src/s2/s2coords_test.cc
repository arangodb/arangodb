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

#include "s2/s2coords.h"

#include <cmath>

#include <gtest/gtest.h>
#include "s2/s2cell_id.h"
#include "s2/s2coords_internal.h"
#include "s2/s2testing.h"

using std::fabs;
using S2::internal::kSwapMask;
using S2::internal::kInvertMask;
using S2::internal::kIJtoPos;
using S2::internal::kPosToIJ;

inline static int SwapAxes(int ij) {
  return ((ij >> 1) & 1) + ((ij & 1) << 1);
}

inline static int InvertBits(int ij) {
  return ij ^ 3;
}

TEST(S2, TraversalOrder) {
  for (int r = 0; r < 4; ++r) {
    for (int i = 0; i < 4; ++i) {
      // Check consistency with respect to swapping axes.
      EXPECT_EQ(kIJtoPos[r][i],
                kIJtoPos[r ^ kSwapMask][SwapAxes(i)]);
      EXPECT_EQ(kPosToIJ[r][i],
                SwapAxes(kPosToIJ[r ^ kSwapMask][i]));

      // Check consistency with respect to reversing axis directions.
      EXPECT_EQ(kIJtoPos[r][i],
                kIJtoPos[r ^ kInvertMask][InvertBits(i)]);
      EXPECT_EQ(kPosToIJ[r][i],
                InvertBits(kPosToIJ[r ^ kInvertMask][i]));

      // Check that the two tables are inverses of each other.
      EXPECT_EQ(kIJtoPos[r][kPosToIJ[r][i]], i);
      EXPECT_EQ(kPosToIJ[r][kIJtoPos[r][i]], i);
    }
  }
}

TEST(S2, ST_UV_Conversions) {
  // Check boundary conditions.
  for (double s = 0; s <= 1; s += 0.5) {
    volatile double u = S2::STtoUV(s);
    EXPECT_EQ(u, 2 * s - 1);
  }
  for (double u = -1; u <= 1; ++u) {
    volatile double s = S2::UVtoST(u);
    EXPECT_EQ(s, 0.5 * (u + 1));
  }
  // Check that UVtoST and STtoUV are inverses.
  for (double x = 0; x <= 1; x += 0.0001) {
    EXPECT_NEAR(S2::UVtoST(S2::STtoUV(x)), x, 1e-15);
    EXPECT_NEAR(S2::STtoUV(S2::UVtoST(2*x-1)), 2*x-1, 1e-15);
  }
}

TEST(S2, FaceUVtoXYZ) {
  // Check that each face appears exactly once.
  S2Point sum;
  for (int face = 0; face < 6; ++face) {
    S2Point center = S2::FaceUVtoXYZ(face, 0, 0);
    EXPECT_EQ(S2::GetNorm(face), center);
    EXPECT_EQ(fabs(center[center.LargestAbsComponent()]), 1);
    sum += center.Fabs();
  }
  EXPECT_EQ(sum, S2Point(2, 2, 2));

  // Check that each face has a right-handed coordinate system.
  for (int face = 0; face < 6; ++face) {
    EXPECT_EQ(S2::GetUAxis(face).CrossProd(S2::GetVAxis(face))
              .DotProd(S2::FaceUVtoXYZ(face, 0, 0)), 1);
  }

  // Check that the Hilbert curves on each face combine to form a
  // continuous curve over the entire cube.
  for (int face = 0; face < 6; ++face) {
    // The Hilbert curve on each face starts at (-1,-1) and terminates
    // at either (1,-1) (if axes not swapped) or (-1,1) (if swapped).
    int sign = (face & kSwapMask) ? -1 : 1;
    EXPECT_EQ(S2::FaceUVtoXYZ(face, sign, -sign),
              S2::FaceUVtoXYZ((face + 1) % 6, -1, -1));
  }
}

TEST(S2, FaceXYZtoUVW) {
  for (int face = 0; face < 6; ++face) {
    EXPECT_EQ(S2Point( 0,  0,  0), S2::FaceXYZtoUVW(face, S2Point(0, 0, 0)));
    EXPECT_EQ(S2Point( 1,  0,  0), S2::FaceXYZtoUVW(face,  S2::GetUAxis(face)));
    EXPECT_EQ(S2Point(-1,  0,  0), S2::FaceXYZtoUVW(face, -S2::GetUAxis(face)));
    EXPECT_EQ(S2Point( 0,  1,  0), S2::FaceXYZtoUVW(face,  S2::GetVAxis(face)));
    EXPECT_EQ(S2Point( 0, -1,  0), S2::FaceXYZtoUVW(face, -S2::GetVAxis(face)));
    EXPECT_EQ(S2Point( 0,  0,  1), S2::FaceXYZtoUVW(face,  S2::GetNorm(face)));
    EXPECT_EQ(S2Point( 0,  0, -1), S2::FaceXYZtoUVW(face, -S2::GetNorm(face)));
  }
}

TEST(S2, XYZToFaceSiTi) {
  // Check the conversion of random cells to center points and back.
  for (int level = 0; level <= S2CellId::kMaxLevel; ++level) {
    for (int i = 0; i < 1000; ++i) {
      S2CellId id = S2Testing::GetRandomCellId(level);
      int face;
      unsigned int si, ti;
      int actual_level = S2::XYZtoFaceSiTi(id.ToPoint(), &face, &si, &ti);
      EXPECT_EQ(level, actual_level);
      S2CellId actual_id =
          S2CellId::FromFaceIJ(face, si / 2, ti / 2).parent(level);
      EXPECT_EQ(id, actual_id);

      // Now test a point near the cell center but not equal to it.
      S2Point p_moved = id.ToPoint() + S2Point(1e-13, 1e-13, 1e-13);
      int face_moved;
      unsigned int si_moved, ti_moved;
      actual_level = S2::XYZtoFaceSiTi(p_moved, &face_moved, &si_moved,
                                       &ti_moved);
      EXPECT_EQ(-1, actual_level);
      EXPECT_EQ(face, face_moved);
      EXPECT_EQ(si, si_moved);
      EXPECT_EQ(ti, ti_moved);

      // Finally, test some random (si,ti) values that may be at different
      // levels, or not at a valid level at all (for example, si == 0).
      int face_random = S2Testing::rnd.Uniform(S2CellId::kNumFaces);
      unsigned int si_random, ti_random;
      unsigned int mask = -1 << (S2CellId::kMaxLevel - level);
      do {
        si_random = S2Testing::rnd.Rand32() & mask;
        ti_random = S2Testing::rnd.Rand32() & mask;
      } while (si_random > S2::kMaxSiTi || ti_random > S2::kMaxSiTi);
      S2Point p_random = S2::FaceSiTitoXYZ(face_random, si_random, ti_random);
      actual_level = S2::XYZtoFaceSiTi(p_random, &face, &si, &ti);
      if (face != face_random) {
        // The chosen point is on the edge of a top-level face cell.
        EXPECT_EQ(-1, actual_level);
        EXPECT_TRUE(si == 0 || si == S2::kMaxSiTi ||
                    ti == 0 || ti == S2::kMaxSiTi);
      } else {
        EXPECT_EQ(si_random, si);
        EXPECT_EQ(ti_random, ti);
        if (actual_level >= 0) {
          EXPECT_EQ(p_random,
                    S2CellId::FromFaceIJ(face, si / 2, ti / 2).
                    parent(actual_level).ToPoint());
        }
      }
    }
  }
}

TEST(S2, UVNorms) {
  // Check that GetUNorm and GetVNorm compute right-handed normals for
  // an edge in the increasing U or V direction.
  for (int face = 0; face < 6; ++face) {
    for (double x = -1; x <= 1; x += 1/1024.) {
      EXPECT_DOUBLE_EQ(S2::FaceUVtoXYZ(face, x, -1)
                       .CrossProd(S2::FaceUVtoXYZ(face, x, 1))
                       .Angle(S2::GetUNorm(face, x)), 0);
      EXPECT_DOUBLE_EQ(S2::FaceUVtoXYZ(face, -1, x)
                       .CrossProd(S2::FaceUVtoXYZ(face, 1, x))
                       .Angle(S2::GetVNorm(face, x)), 0);
    }
  }
}

TEST(S2, UVWAxis) {
  for (int face = 0; face < 6; ++face) {
    // Check that axes are consistent with FaceUVtoXYZ.
    EXPECT_EQ(S2::FaceUVtoXYZ(face, 1, 0) - S2::FaceUVtoXYZ(face, 0, 0),
              S2::GetUAxis(face));
    EXPECT_EQ(S2::FaceUVtoXYZ(face, 0, 1) - S2::FaceUVtoXYZ(face, 0, 0),
              S2::GetVAxis(face));
    EXPECT_EQ(S2::FaceUVtoXYZ(face, 0, 0), S2::GetNorm(face));

    // Check that every face coordinate frame is right-handed.
    EXPECT_EQ(1, S2::GetUAxis(face).CrossProd(S2::GetVAxis(face))
              .DotProd(S2::GetNorm(face)));

    // Check that GetUVWAxis is consistent with GetUAxis, GetVAxis, GetNorm.
    EXPECT_EQ(S2::GetUAxis(face), S2::GetUVWAxis(face, 0));
    EXPECT_EQ(S2::GetVAxis(face), S2::GetUVWAxis(face, 1));
    EXPECT_EQ(S2::GetNorm(face), S2::GetUVWAxis(face, 2));
  }
}

TEST(S2, UVWFace) {
  // Check that GetUVWFace is consistent with GetUVWAxis.
  for (int face = 0; face < 6; ++face) {
    for (int axis = 0; axis < 3; ++axis) {
      EXPECT_EQ(S2::GetFace(-S2::GetUVWAxis(face, axis)),
                S2::GetUVWFace(face, axis, 0));
      EXPECT_EQ(S2::GetFace(S2::GetUVWAxis(face, axis)),
                S2::GetUVWFace(face, axis, 1));
    }
  }
}
