////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#include "utils/bit_utils.hpp"
#include "utils/numeric_utils.hpp"
#include <vector>
#include <algorithm>

namespace {

template<typename T>
iresearch::bstring encode(T value, size_t offset = 0) {
  typedef iresearch::numeric_utils::numeric_traits<T> traits_t;
  iresearch::bstring data;
  data.resize(traits_t::size());
  traits_t::encode(traits_t::integral(value), &(data[0]), offset);
  return data;
}

}

TEST(numeric_utils_test, encode32) {
  const irs::byte_type TYPE_MAGIC = 0;

  // 0
  {
    const uint32_t value = 0;
    // shift = 0
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 8 
    {
      const size_t shift = 8;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 24 
    {
      const size_t shift = 24;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 43
    {
      const size_t shift = 43;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode32(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint32_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode32(actual.data()));
    }
  }

  // 1 byte
  {
    const uint32_t value = 0x7D;
    // shift = 0
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0x7D);
      ASSERT_EQ(actual, expected);
    }

    // shift = 8
    {
      const size_t shift = 8;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 24 
    {
      const size_t shift = 24;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 1132 
    {
      const size_t shift = 1132;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode32(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint32_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode32(actual.data()));
    }
  }

  // 2 bytes
  {
    const uint32_t value = 0x3037;
    
    // shift = 0
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0x30);
      expected.append(1, 0x37);
      ASSERT_EQ(actual, expected);
    }

    // shift = 8
    {
      const size_t shift = 8;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0x30);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 24 
    {
      const size_t shift = 24;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // invalid shift = 54 
    {
      const size_t shift = 54;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode32(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint32_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode32(actual.data()));
    }
  }

  // 3 bytes
  {
    const uint32_t value = 0x44007D;

    // shift = 0
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0])));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0x44);
      expected.append(1, 0);
      expected.append(1, 0x7D);
      ASSERT_EQ(actual, expected);
    }

    // shift = 8
    {
      const size_t shift = 8;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0x44);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0x44);
      ASSERT_EQ(actual, expected);
    }

    // shift = 24 
    {
      const size_t shift = 24;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // invalid shift = 33 
    {
      const size_t shift = 33;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode32(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint32_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode32(actual.data()));
    }
  }

  // 4 bytes
  {
    const uint32_t value = 0x7FFFFEAF;

    // shift = 0
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xFF);
      expected.append(1, 0xFF);
      expected.append(1, 0xFE);
      expected.append(1, 0xAF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 8
    {
      const size_t shift = 8;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xFF);
      expected.append(1, 0xFF);
      expected.append(1, 0xFE);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xFF);
      expected.append(1, 0xFF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 24 
    {
      const size_t shift = 24;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xFF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // invalid shift = 33
    {
      const size_t shift = 33;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode32(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint32_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int32_t>::size());
      actual.resize(irs::numeric_utils::encode32(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode32(actual.data()));
    }
  }
}

TEST(numeric_utils_test, i32_lexicographical_sort) {
  std::vector<int32_t> data = {
    -350, -761, -609, -343, 681,
    -915, -166, -727, 144, -464,
    486, -527, 161, -616, -823,
    -283, -345, -988, 208, -550,
    856, -235, 582, -357, 417,
    565, -393, 658, 147, 266,
    242, 990, -604, 686, 982,
    287, 950, -566, 767, -25,
    503, -214, 70, 351, 473,
    -531, -654, -795, 938, 634,
    -188, 437, 57, -808, -129,
    -793, 202, -671, -859, -808,
    245, 71, -968, 533, -179,
    771, -954, 839, -49, 776,
    -875, 761, -327, -947, 979,
    -129, -291, 835, 358, -796,
    -338, -168, -669, -181, -550,
    -368, 747, 321, -181, -243,
    -540, -974, -838, 266, -495,
    77, 660, -911, -957, -418
  };

  ASSERT_FALSE(std::is_sorted(data.begin(), data.end()));

  // sort values as a strings 
  {
    irs::bstring lhsb, rhsb;
    lhsb.resize(iresearch::numeric_utils::numeric_traits<int32_t>::size());
    rhsb.resize(iresearch::numeric_utils::numeric_traits<int32_t>::size());

    std::sort(
      data.begin(), data.end(),
      [&lhsb, &rhsb] (int32_t lhs, int32_t rhs) {
        lhsb.resize(iresearch::numeric_utils::encode32(lhs, &(lhsb[0])));
        rhsb.resize(iresearch::numeric_utils::encode32(rhs, &(rhsb[0])));
        return lhsb < rhsb;
    });
  }

  ASSERT_TRUE(std::is_sorted(data.begin(), data.end()));
}

TEST(numeric_utils_test, float_lexicographical_sort) {
  std::vector<float_t> data = {
    651986.24597f, 0.f, 789897.54621f, 334163.97801f, 503825.00775f, 145063.42199f, 860891.64170f, 70621.40595f, 652100.77900f, 197530.50436f, 
    515243.79748f, 430713.86816f, 920883.39861f, 33127.12690f, 669809.53751f, 742514.67643f, 541261.20072f, 424864.54813f, -84206.11661f, 
    37734.94341f, 399434.40440f, 583076.69427f, 921670.79766f, 370855.67353f, 539653.46945f, 888530.51518f, 955485.29949f, 728637.62999f, 
    604241.69549f, 199548.03446f, 987841.11798f, 187679.83587f, 860653.96767f, 704908.54946f, 270492.27179f, 981837.20772f, 89069.58880f, 
    976493.74761f, 177264.69379f, 580679.70200f, 269226.75505f, -7565.20355f, 580326.52632f, 820924.88651f, 999188.46427f, 208690.17207f, 
    -65341.05952f, 415085.10025f, 249832.76681f, 761120.91782f, 221577.50140f, 883788.22463f, 487001.46581f, 231184.95100f, 313349.23483f, 
    58377.55974f, -24389.93795f, -85784.41119f, 230454.79233f, 40357.24455f, 775261.89865f, 802026.18120f, 748156.61351f, 565290.75590f, std::numeric_limits<float_t>::max(),
    59125.49834f, 81581.64508f, -77242.32905f, 358950.49929f, 927754.07961f, 816446.24556f, 491053.68666f, 25909.79241f, 319106.64948f, std::numeric_limits<float_t>::min(),
    798202.10184f, 318100.23017f, 183748.53501f, 891857.49166f, 638973.58074f, -53220.76564f, 23770.88234f, 223292.54636f, 59219.16834f, -1*std::numeric_limits<float_t>::infinity(),
    556780.22390f, 625542.69411f, 943141.14072f, 299615.24480f, -49206.87529f, 292463.48822f, 786073.54881f, 27863.51332f, 473998.82589f, std::numeric_limits<float_t>::infinity(),
    391216.34299f, 945374.63819f, 958320.88699f, 826033.74749f, 439145.14628f, 132385.51939f, 420388.33719f, 878636.29916f, 164969.74829f, 763102.14175f
  };

  ASSERT_FALSE(std::is_sorted(data.begin(), data.end()));
  std::sort(data.begin(), data.end());

  // sort values as a strings 
  {
    iresearch::bstring lhsb, rhsb;
    lhsb.resize(iresearch::numeric_utils::numeric_traits<int32_t>::size());
    rhsb.resize(iresearch::numeric_utils::numeric_traits<int32_t>::size());

    std::sort(
      data.begin(), data.end(),
      [&lhsb, &rhsb] (float_t lhs, float_t rhs) {
        lhsb.resize(iresearch::numeric_utils::encode32(iresearch::numeric_utils::ftoi32(lhs), &(lhsb[0])));
        rhsb.resize(iresearch::numeric_utils::encode32(iresearch::numeric_utils::ftoi32(rhs), &(rhsb[0])));
        return lhsb < rhsb;
    });
  }

  ASSERT_TRUE(std::is_sorted(data.begin(), data.end()));
}

TEST(numeric_utils_test, i64_lexicographical_sort) {
  std::vector<int64_t> data = {
    75916234875, 52789213189, -351958887,95319743962, 79430098384, -93388245157, -49766133093, -14684623510, 81939936346, -88743499960, 
    20121855753, 83100432533, 51334979222, -73041310283, 61053586489, -27283259777, 66875916011, 64196723717, -63245085177, 49356623092, 
    4831901977, -94997824110, 558743111, 86064753122, -20505155546, 97553550685, 4184837506, -87917522122, -18266405289, 72779316345, 
    78725060901, -37948627940, -48856097607, 53160149963, -71272992316, -20828322249, 9522187027, -84578726831, 12441908766, 17798626822, 
    -20379673205, -18890364139, 2691328827, -26486115883, 68027701743, 91157000176, 67489735701, -65819861863, 32420845326, 35357628656, 
    -352937797, 3730286026, -48575671214, -57132400233, -57931671060, 79274974121, 9443962937, 51250545543, -53668161995, 63749872762, 
    -10243160821, 69057817471, -32302480156, 55309984087, 26821242343, -47858619974, -43658945958, 13882630371, -54803336237, 
    -42915650818, 30843782668, 81271509976, -62481641673, 58881660576, -18924990310, -67739865224, 62103715004, 86468068247, 97226357283, 
    79135831106, 32583755567, -51070913529, 84103329088, 55669172603, -80203080540, 76458883725, 89479791488, 71316492715, 77391498575, 
    -40379985779, 40912438277, -74300496300, 87642158905, -64141659318, 49343408029, 53715985320, -79770447588, -74497633642, -61100778565, 20399307283
  };

  ASSERT_FALSE(std::is_sorted(data.begin(), data.end()));

  // sort values as a strings 
  {
    irs::bstring lhsb, rhsb;
    lhsb.resize(iresearch::numeric_utils::numeric_traits<int64_t>::size());
    rhsb.resize(iresearch::numeric_utils::numeric_traits<int64_t>::size());

    std::sort(
      data.begin(), data.end(),
      [&lhsb, &rhsb] (int64_t lhs, int64_t rhs) {
        lhsb.resize(iresearch::numeric_utils::encode64(lhs, &(lhsb[0])));
        rhsb.resize(iresearch::numeric_utils::encode64(rhs, &(rhsb[0])));
        return lhsb < rhsb;
    });
  }

  ASSERT_TRUE(std::is_sorted(data.begin(), data.end()));
}

TEST(numeric_utils_test, double_lexicographical_sort) {
  std::vector<double_t> data = {
    0., 317957780006.32197596427145, 533992104946.63198708865418, -444498986678.43033870609027, 993156979788.63351968519088, 609230681140.54886677327979, 171177411065.98517441469486, 
    -940116899991.46242625613810, 72774954639.73607618349422, -217026020966.94801979090461, -925157835672.21734471256721, 767751105952.89203615528161, 598978092707.21771414727797, 
    -655278929348.69924995521980, -557099107446.66080337327942, -727673216596.09359530550129, 155929101237.99420019518314, -798911902959.88316785538717, 584117345318.25284721248450, 
    -181281342721.20955526885090, -884976572303.55617231854991, -737155568663.56896639967755, -259257466187.35485998324811, 724842924496.55147478755167, 992409605529.35004491794391, 
    -427341238328.88027575280530, 957748176510.32850915115723, -927854884382.26975704648986, -295874505907.23869665862932, -298929529869.43047953277383, 107495609255.27271314303983, 
    102434974677.13196514040789, -526793124865.17854261453196, -986666726873.56673501132370, -105069548406.20492976447797, -810126205817.85505908441500, 502725540428.76024750469264, 
    -86871683172.35618977451520, 734071867416.64626980975562, -777490837395.88541788788765, -488500977628.16631124735172, 463763000193.87295478669598, -226003491890.61834099265670, 
    761498090700.01267394982869, 798989307041.74065359017842, -905445250638.50230101426239, -602929648758.34512000826426, -196635976059.65844172037134, -68902930742.55014338649350, 
    863617225486.60693014348248, 297576823876.04230264017466, 997953300363.36244100861365, 30857690158.69949486046075, -443412200288.57337324347970, 219426915617.39282478456983, 
    104728098541.83723150838038, -475445743405.93337239973870, 772038568636.42044767570703, -340372866643.76634482469705, -84314886985.49330559814968, -413809077541.25496258086290, 
    125388728047.43644224826081, 999016783199.74652640509723, -689317751531.17149673922523, 438800651318.76647999452729, -458810955965.33778867001542, -86436450987.32619126668488, 
    -331705479571.45864124012116, -183427201203.73517330909854, -519768203385.06633573447649, 517973236515.17426432816976, 502506143181.82046673345401, -789732965542.81048734803241, 
    -919346901550.58489253306058, 10824521542.91072932207525, -745318840139.22831050084360, -32521082569.15634617635810, 705549213898.15826616164216, 980234109787.37944261514556, 
    941403018283.37973835104133, 361903082282.23728122293823, 559399746153.22414140832803, 64887538116.83856794463404, 531879426693.48764544980956, -767469304505.48851141030365, 
    454718537374.73419745207494, 26781561331.25608848932017, -802390430961.91735517322894, -323002730180.97632107370362, -849713418562.76309982070844, -439276315942.06966270789023, 
    -222411432872.71793599832707, 125254418293.59830277673821, 62915394577.62399435864016, -250289681949.78762508825754, -818758496930.24461014673328, -168764751948.77234843968057, 
    -418952390280.99569970788234, 6143003239.36296777770061, -683268292659.55290415303451, 448388662863.70375327007088, std::numeric_limits<double_t>::infinity(), 
    -1*std::numeric_limits<double_t>::infinity(), std::numeric_limits<double_t>::max(), std::numeric_limits<double_t>::min()
  };

  ASSERT_FALSE(std::is_sorted(data.begin(), data.end()));
  std::sort(data.begin(), data.end());

  // sort values as a strings 
  {
    typedef iresearch::numeric_utils::numeric_traits<double_t> traits_t;

    irs::bstring lhsb, rhsb;
    lhsb.resize(traits_t::size());
    rhsb.resize(traits_t::size());

    std::sort(
      data.begin(), data.end(),
      [&lhsb, &rhsb] (double_t lhs, double_t rhs) {
        lhsb.resize(traits_t::encode(traits_t::integral(lhs), &(lhsb[0])));
        rhsb.resize(traits_t::encode(traits_t::integral(rhs), &(rhsb[0])));
        return lhsb < rhsb;
    });
  }

  ASSERT_TRUE(std::is_sorted(data.begin(), data.end()));
}

TEST(numeric_utils_test, encode64) {
  const irs::byte_type TYPE_MAGIC = 0x60;

  // 0
  {
    const uint64_t value = 0;
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }

  // 1 byte
  {
    const uint64_t value = 0x7D;
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0x7D);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }

  // 2 bytes
  {
    const uint64_t value = 0x3037;
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0x30);
      expected.append(1, 0x37);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }

  // 3 bytes
  {
    const uint64_t value = 0x44007D;
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0x44);
      expected.append(1, 0);
      expected.append(1, 0x7D);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0x44);

      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }

  // 4 bytes
  {
    const uint64_t value = 0x7FFFFEAF;
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      expected.append(1, 0xFE);
      expected.append(1, 0xAF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }

  // 5 bytes
  {
    const uint64_t value = uint64_t(0xCE7FFFFEAF);
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0xCE);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      expected.append(1, 0xFE);
      expected.append(1, 0xAF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0xCE);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0);
      expected.append(1, 0xCE);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }

  // 6 bytes
  {
    const uint64_t value = uint64_t(0xFACE7FFFFEAF);
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      expected.append(1, 0xFE);
      expected.append(1, 0xAF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }

  // 7 bytes
  {
    const uint64_t value = uint64_t(0xEFFACE7FFFFEAF);
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0xEF);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      expected.append(1, 0xFE);
      expected.append(1, 0xAF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0xEF);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0xEF);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      expected.append(1, 0xEF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0x80);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }

  // 8 bytes
  {
    const uint64_t value = uint64_t(0x2AEFFACE7FFFFEAF);
    // shift = 0 
    {
      const size_t shift = 0;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xAA);
      expected.append(1, 0xEF);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      expected.append(1, 0xFE);
      expected.append(1, 0xAF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 16 
    {
      const size_t shift = 16;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xAA);
      expected.append(1, 0xEF);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      expected.append(1, 0x7F);
      expected.append(1, 0xFF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 32 
    {
      const size_t shift = 32;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xAA);
      expected.append(1, 0xEF);
      expected.append(1, 0xFA);
      expected.append(1, 0xCE);
      ASSERT_EQ(actual, expected);
    }

    // shift = 48 
    {
      const size_t shift = 48;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xAA);
      expected.append(1, 0xEF);
      ASSERT_EQ(actual, expected);
    }

    // shift = 56 
    {
      const size_t shift = 56;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      expected.append(1, 0xAA);
      ASSERT_EQ(actual, expected);
    }

    // shift = 64 
    {
      const size_t shift = 64;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
    }

    // shift = 65 
    {
      const size_t shift = 65;
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      irs::bstring expected;
      expected.append(1, TYPE_MAGIC + static_cast<irs::byte_type>(shift));
      ASSERT_EQ(actual, expected);
      ASSERT_EQ(0, irs::numeric_utils::decode64(actual.data()));
    }

    for (size_t shift = 0; shift < irs::bits_required<uint64_t>(); ++shift) {
      irs::bstring actual;
      actual.resize(irs::numeric_utils::numeric_traits<int64_t>::size());
      actual.resize(irs::numeric_utils::encode64(value, &(actual[0]), shift));
      ASSERT_EQ(((value >> shift) << shift), irs::numeric_utils::decode64(actual.data()));
    }
  }
}

TEST(numeric_utils_test, int_traits) {
  typedef irs::numeric_utils::numeric_traits<int32_t> traits_t;
  typedef traits_t::integral_t type;
  ASSERT_EQ(
    std::numeric_limits<type>::min(), 
    traits_t::decode(traits_t::min().c_str())
  );
  ASSERT_EQ(
    std::numeric_limits<type>::max(), 
    traits_t::decode(traits_t::max().c_str())
  );
  ASSERT_EQ(5, traits_t::size());

  {
    auto encoded = encode(std::numeric_limits<type>::min());
    ASSERT_EQ(
      std::numeric_limits<type>::min(), 
      traits_t::decode(encoded.c_str())
    );
  }

  {
    auto encoded = encode(std::numeric_limits<type>::max());
    ASSERT_EQ(
      std::numeric_limits<type>::max(), 
      traits_t::decode(encoded.c_str())
    );
  }

  {
    auto encoded = encode(INT32_C(0));
    ASSERT_EQ(
      INT32_C(0),
      traits_t::decode(encoded.c_str())
    );
  }
}

TEST(numeric_utils_test, uint_traits) {
  typedef irs::numeric_utils::numeric_traits<uint32_t> traits_t;
  typedef traits_t::integral_t type;
  ASSERT_EQ(
    std::numeric_limits<type>::min(), 
    traits_t::decode(traits_t::min().c_str())
  );
  ASSERT_EQ(
    std::numeric_limits<type>::max(), 
    traits_t::decode(traits_t::max().c_str())
  );
  ASSERT_EQ(5, traits_t::size());

  {
    auto encoded = encode(std::numeric_limits<type>::min());
    ASSERT_EQ(
      std::numeric_limits<type>::min(), 
      traits_t::decode(encoded.c_str())
    );
  }

  {
    auto encoded = encode(std::numeric_limits<type>::max());
    ASSERT_EQ(
      std::numeric_limits<type>::max(), 
      traits_t::decode(encoded.c_str())
    );
  }

  {
    auto encoded = encode(INT32_C(0));
    ASSERT_EQ(
      INT32_C(0),
      traits_t::decode(encoded.c_str())
    );
  }

  {
    traits_t::integral_t value(0x12345678);

    if (irs::numeric_utils::is_big_endian()) {
      ASSERT_EQ(value, traits_t::hton(value));
      ASSERT_EQ(value, traits_t::ntoh(value));
    } else {
      ASSERT_NE(value, traits_t::hton(value));
      ASSERT_NE(value, traits_t::ntoh(value));
      ASSERT_EQ(value, traits_t::hton(traits_t::ntoh(value)));
      ASSERT_EQ(value, traits_t::ntoh(traits_t::hton(value)));
    }
  }
}

TEST(numeric_utils_test, long_traits) {
  typedef irs::numeric_utils::numeric_traits<int64_t> traits_t;
  typedef traits_t::integral_t type;
  ASSERT_EQ(
    std::numeric_limits<type>::min(), 
    traits_t::decode(traits_t::min().c_str())
  );
  ASSERT_EQ(
    std::numeric_limits<type>::max(), 
    traits_t::decode(traits_t::max().c_str())
  );
  ASSERT_EQ(9, traits_t::size());

  {
    auto encoded = encode(std::numeric_limits<type>::min());
    ASSERT_EQ(
      std::numeric_limits<type>::min(), 
      traits_t::decode(encoded.c_str())
    );
  }

  {
    auto encoded = encode(std::numeric_limits<type>::max());
    ASSERT_EQ(
      std::numeric_limits<type>::max(), 
      traits_t::decode(encoded.c_str())
    );
  }

  {
    auto encoded = encode(INT64_C(0));
    ASSERT_EQ(
      INT64_C(0),
      traits_t::decode(encoded.c_str())
    );
  }
}

TEST(numeric_utils_test, ulong_traits) {
  typedef irs::numeric_utils::numeric_traits<uint64_t> traits_t;
  typedef traits_t::integral_t type;
  ASSERT_EQ(
    std::numeric_limits<type>::min(), 
    traits_t::decode(traits_t::min().c_str())
  );
  ASSERT_EQ(
    std::numeric_limits<type>::max(), 
    traits_t::decode(traits_t::max().c_str())
  );
  ASSERT_EQ(9, traits_t::size());

  {
    auto encoded = encode(std::numeric_limits<type>::min());
    ASSERT_EQ(
      std::numeric_limits<type>::min(), 
      traits_t::decode(encoded.c_str())
    );
  }

  {
    auto encoded = encode(std::numeric_limits<type>::max());
    ASSERT_EQ(
      std::numeric_limits<type>::max(), 
      traits_t::decode(encoded.c_str())
    );
  }

  {
    auto encoded = encode(INT64_C(0));
    ASSERT_EQ(
      INT64_C(0),
      traits_t::decode(encoded.c_str())
    );
  }

  {
    traits_t::integral_t value(0x1234567890ABCDEF);

    if (irs::numeric_utils::is_big_endian()) {
      ASSERT_EQ(value, traits_t::hton(value));
      ASSERT_EQ(value, traits_t::ntoh(value));
    } else {
      ASSERT_NE(value, traits_t::hton(value));
      ASSERT_NE(value, traits_t::ntoh(value));
      ASSERT_EQ(value, traits_t::hton(traits_t::ntoh(value)));
      ASSERT_EQ(value, traits_t::ntoh(traits_t::hton(value)));
    }
  }
}

TEST(numeric_utils_test, float_t_traits) {
  typedef float_t type;
  typedef irs::numeric_utils::numeric_traits<type> traits_t;
  ASSERT_EQ(
    std::numeric_limits<type>::min(), 
    traits_t::decode(traits_t::min().c_str())
  );
  ASSERT_EQ(
    std::numeric_limits<type>::max(), 
    traits_t::decode(traits_t::max().c_str())
  );
  ASSERT_EQ(
    std::numeric_limits<type>::infinity(), 
    traits_t::decode(traits_t::inf().c_str())
  );
  ASSERT_EQ(
    -1*std::numeric_limits<type>::infinity(), 
    traits_t::decode(traits_t::ninf().c_str())
  );
  ASSERT_EQ(5, traits_t::size());

  {
    auto encoded = encode(std::numeric_limits<type>::min());
    ASSERT_EQ(
      std::numeric_limits<type>::min(), 
      traits_t::decode(encoded.c_str())
    );
  }
  {
    auto encoded = encode(std::numeric_limits<type>::max());
    ASSERT_EQ(
      std::numeric_limits<type>::max(), 
      traits_t::decode(encoded.c_str())
    );
  }
  {
    auto encoded = encode(std::numeric_limits<type>::infinity());
    ASSERT_EQ(
      std::numeric_limits<type>::infinity(), 
      traits_t::decode(encoded.c_str())
    );
  }
  {
    auto encoded = encode(-1*std::numeric_limits<type>::infinity());
    ASSERT_EQ(
      -1*std::numeric_limits<type>::infinity(), 
      traits_t::decode(encoded.c_str())
    );
  }
  {
    auto encoded = encode((float_t)0.f);
    ASSERT_EQ(
      0.f,
      traits_t::decode(encoded.c_str())
    );
  }
}

TEST(numeric_utils_test, double_t_traits) {
  typedef double_t type;
  typedef irs::numeric_utils::numeric_traits<type> traits_t;
  ASSERT_EQ(
    std::numeric_limits<type>::min(), 
    traits_t::decode(traits_t::min().c_str())
  );
  ASSERT_EQ(
    std::numeric_limits<type>::max(), 
    traits_t::decode(traits_t::max().c_str())
  );
  ASSERT_EQ(
    std::numeric_limits<type>::infinity(), 
    traits_t::decode(traits_t::inf().c_str())
  );
  ASSERT_EQ(
    -1*std::numeric_limits<type>::infinity(), 
    traits_t::decode(traits_t::ninf().c_str())
  );
  ASSERT_EQ(9, traits_t::size());

  {
    auto encoded = encode(std::numeric_limits<type>::min());
    ASSERT_EQ(
      std::numeric_limits<type>::min(), 
      traits_t::decode(encoded.c_str())
    );
  }
  {
    auto encoded = encode(std::numeric_limits<type>::max());
    ASSERT_EQ(
      std::numeric_limits<type>::max(), 
      traits_t::decode(encoded.c_str())
    );
  }
  {
    auto encoded = encode(std::numeric_limits<type>::infinity());
    ASSERT_EQ(
      std::numeric_limits<type>::infinity(), 
      traits_t::decode(encoded.c_str())
    );
  }
  {
    auto encoded = encode(-1*std::numeric_limits<type>::infinity());
    ASSERT_EQ(
      -1*std::numeric_limits<type>::infinity(), 
      traits_t::decode(encoded.c_str())
    );
  }
  {
    auto encoded = encode((double_t)0.);
    ASSERT_EQ(
      0.,
      traits_t::decode(encoded.c_str())
    );
  }
}

TEST(numeric_utils_test, float_traits) {
  typedef float type;
  typedef irs::numeric_utils::numeric_traits<type> traits_t;

}

TEST(numeric_utils_test, double_traits) {
  typedef double type;
  typedef irs::numeric_utils::numeric_traits<type> traits_t;

}

TEST(numeric_utils_test, long_double_traits) {
  typedef long double type;
  typedef irs::numeric_utils::numeric_traits<type> traits_t;

}
