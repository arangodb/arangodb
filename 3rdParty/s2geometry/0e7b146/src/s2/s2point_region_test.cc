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

#include "s2/s2point_region.h"

#include <memory>

#include <gtest/gtest.h>
#include "s2/util/coding/coder.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"

using std::unique_ptr;

namespace {

TEST(S2PointRegionTest, Basic) {
  S2Point p(1, 0, 0);
  S2PointRegion r0(p);
  EXPECT_EQ(r0.point(), p);
  EXPECT_TRUE(r0.Contains(p));
  EXPECT_TRUE(r0.Contains(r0.point()));
  EXPECT_FALSE(r0.Contains(S2Point(1, 0, 1)));
  unique_ptr<S2PointRegion> r0_clone(r0.Clone());
  EXPECT_EQ(r0_clone->point(), r0.point());
  EXPECT_EQ(r0.GetCapBound(), S2Cap::FromPoint(p));
  S2LatLng ll(p);
  EXPECT_EQ(r0.GetRectBound(), S2LatLngRect(ll, ll));

  // The leaf cell containing a point is still much larger than the point.
  S2Cell cell(p);
  EXPECT_FALSE(r0.Contains(cell));
  EXPECT_TRUE(r0.MayIntersect(cell));
}

TEST(S2PointRegionTest, EncodeAndDecode) {
  S2Point p(.6, .8, 0);
  S2PointRegion r(p);

  Encoder encoder;
  r.Encode(&encoder);

  Decoder decoder(encoder.base(), encoder.length());
  S2PointRegion decoded_r(S2Point(1, 0, 0));
  decoded_r.Decode(&decoder);
  S2Point decoded_p = decoded_r.point();

  EXPECT_EQ(0.6, decoded_p[0]);
  EXPECT_EQ(0.8, decoded_p[1]);
  EXPECT_EQ(0.0, decoded_p[2]);
}

TEST(S2PointRegionTest, DecodeUnitLength) {
  // Ensure that we have the right format for the next test.
  Encoder encoder;
  encoder.Ensure(1 + 3 * 8);

  encoder.put8(1);  // version number
  encoder.putdouble(1.0);
  encoder.putdouble(0.0);
  encoder.putdouble(0.0);

  Decoder decoder(encoder.base(), encoder.length());
  S2PointRegion r(S2Point(-1, 0, 0));
  ASSERT_TRUE(r.Decode(&decoder));
}

TEST(S2PointRegionTest, DecodeNonUnitLength) {
  // Ensure that a decode of a non-unit length vector returns false,
  // rather than S2_DCHECK fails.
  Encoder encoder;
  encoder.Ensure(1 + 3 * 8);

  encoder.put8(1);  // version number
  encoder.putdouble(1.0);
  encoder.putdouble(1.0);
  encoder.putdouble(1.0);

  Decoder decoder(encoder.base(), encoder.length());
  S2PointRegion r(S2Point(-1, 0, 0));
  ASSERT_FALSE(r.Decode(&decoder));
}

}  // namespace
