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

#include "utils/bit_packing.hpp"

#include <vector>
#include <algorithm>
  
using namespace iresearch;

TEST(bit_packing_tests, const_check) {
  // check constants
  ASSERT_EQ(32U, packed::BLOCK_SIZE_32);
  ASSERT_EQ(64U, packed::BLOCK_SIZE_64);
}

TEST(bit_packing_tests, bits_required) {
  // check the number of bits required to store value
  // 32
  ASSERT_EQ(32U, packed::bits_required_32(std::numeric_limits<uint32_t>::max()));
  ASSERT_EQ(25U, packed::bits_required_32(18874368U));
  ASSERT_EQ(20U, packed::bits_required_32(789547U));
  ASSERT_EQ(0, packed::bits_required_32(0U));

  // 64
  ASSERT_EQ(64U, packed::bits_required_64(std::numeric_limits<uint64_t>::max()));
  ASSERT_EQ(58U, packed::bits_required_64(144115188109676544UL));
  ASSERT_EQ(25U, packed::bits_required_64(18874368UL));
  ASSERT_EQ(20U, packed::bits_required_64(789547UL));
  ASSERT_EQ(0, packed::bits_required_64(uint64_t(0)));
}

TEST(bit_packing_tests, blocks_required) {
  // check the number of blocks required to store the number of values
  // 32
  ASSERT_EQ(128UL, packed::blocks_required_32(128U, 32U));
  ASSERT_EQ(1UL, packed::blocks_required_32(1U, 32U));
  ASSERT_EQ(1UL, packed::blocks_required_32(1U, 31U));
  ASSERT_EQ(124UL, packed::blocks_required_32(128U, 31U));
  ASSERT_EQ(68UL, packed::blocks_required_32(128U, 17U));
  ASSERT_EQ(8UL, packed::blocks_required_32(128U, 2U));
  ASSERT_EQ(0UL, packed::blocks_required_32(128U, 0U));
  ASSERT_EQ(127UL, packed::blocks_required_32(127U, 32U));
  ASSERT_EQ(124UL, packed::blocks_required_32(127U, 31U));
  ASSERT_EQ(68UL, packed::blocks_required_32(127U, 17U));
  ASSERT_EQ(30UL, packed::blocks_required_32(73U, 13U));
  ASSERT_EQ(13UL, packed::blocks_required_32(32U, 13U));
  ASSERT_EQ(13UL, packed::blocks_required_32(31U, 13U));

  // 64
  ASSERT_EQ(128UL, packed::blocks_required_64(128U, 64U));
  ASSERT_EQ(1UL, packed::blocks_required_64(1U, 64U));
  ASSERT_EQ(1UL, packed::blocks_required_64(1U, 63U));
  ASSERT_EQ(126UL, packed::blocks_required_64(128U, 63U));
  ASSERT_EQ(64UL, packed::blocks_required_64(128U, 32U));
  ASSERT_EQ(62UL, packed::blocks_required_64(128U, 31U));
  ASSERT_EQ(34UL, packed::blocks_required_64(128U, 17U));
  ASSERT_EQ(4UL, packed::blocks_required_64(128U, 2U));
  ASSERT_EQ(0UL, packed::blocks_required_64(128U, 0U));
  ASSERT_EQ(62UL, packed::blocks_required_64(127U, 31U));
  ASSERT_EQ(15UL, packed::blocks_required_64(73U, 13U));
  ASSERT_EQ(7UL, packed::blocks_required_64(32U, 13U));
  ASSERT_EQ(7UL, packed::blocks_required_64(31U, 13U));
}

TEST(bit_packing_tests, max_value) {
  ASSERT_EQ(std::numeric_limits<uint32_t>::max(), packed::max_value<uint32_t>(32U));
  ASSERT_EQ(0x7FFFFFFF, packed::max_value<uint32_t>(31U));
  ASSERT_EQ(0x1FFFF, packed::max_value<uint32_t>(17U));
  ASSERT_EQ(0x0, packed::max_value<uint32_t>(0U));

  ASSERT_EQ(std::numeric_limits<uint64_t>::max(), packed::max_value<uint64_t>(64U));
  ASSERT_EQ(0x7FFFFFFFFFFFFFFFL, packed::max_value<uint64_t>(63U));
  ASSERT_EQ(0x1FFFFFFFFFFFFFL, packed::max_value<uint64_t>(53U));
  ASSERT_EQ(0x0L, packed::max_value<uint64_t>(uint64_t(0)));
}

TEST(bit_packing_tests, pack_unpack_32) {
  std::vector<uint32_t> src{
    14410, 21766, 15994, 29493, 20819,
    31650, 28103, 27900, 24340, 19822,
    31073, 22825, 22494,  6121, 20200,
    28354, 25256,   220,     2,   393,
      805, 18232,   956, 21480, 20565,
    20500,  4324, 16372,  1064,  9878,
    13639, 18301, 31582, 18341, 30711,
    25801, 16556, 23070,  7921, 20539,
    23571, 27043,  1344, 21307, 25545,
    27844,  4796, 31011,  2238, 21070,
    30090, 16475,  4683, 16839, 14253,
    17744,  5467, 19431, 17022, 24011,
    21970,    24, 21009, 20131,  4494,
     5110,  2991,  8127, 21700, 20629,
    31195, 20423, 24248, 15917, 31151,
     8090, 24170, 11403, 30484, 11747,
    23682,  7179, 21741, 28313, 22102,
    26759,  4385, 19645, 17771,  5977,
    22946,  2705, 15188, 26374, 25695,
    25673,  4866,  8263,  1860, 24033,
    12528, 14297, 11673, 20979,   446,
    31576, 18451, 13478, 11380,  6490,
     2785,  3307,  1912,   382, 24139,
    11483, 16582, 29873, 31287, 18465,
    24084, 29421, 18341, 21654,  3290,
    19579
  };

  src.resize(packed::items_required(src.size()));
  
  // smoke test
  {
    const uint32_t max = *std::max_element(src.begin(), src.end());
    const uint32_t bits = packed::bits_required_32(max);
    const auto expected_bits = packed::bits_required_32(*std::max_element(src.begin(), src.end()));
    ASSERT_EQ(expected_bits, bits);

    std::vector< uint32_t > compress(packed::blocks_required_32(src.size(), bits), 0);
    packed::pack(&src[0], &src[0] + src.size(), &compress[0], bits);

    std::vector< uint32_t > uncompress(src.size());
    packed::unpack(&uncompress[0], &uncompress[0] + uncompress.size(), &compress[0], bits);

    EXPECT_EQ(src.size(), uncompress.size());
    EXPECT_TRUE(std::equal(src.begin(), src.end(), uncompress.begin()));
  }

  // check packing [1..32]
  for (uint32_t bits = 1; bits <= 32; ++bits) {
    std::vector<uint32_t> packed(packed::blocks_required_32(src.size(), bits), 0);
    packed::pack(&src[0], &src[0] + src.size(), &packed[0], bits);

    std::vector<uint32_t> unpacked(src.size());
    packed::unpack(&unpacked[0], &unpacked[0] + unpacked.size(), &packed[0], bits);

    auto comparer = [bits] (uint32_t lhs, uint32_t rhs) {
      // 32 bit left shift cause undefined behavior
      if (32 == bits) {
        return lhs == rhs;
      }

      return (lhs & (1U << bits) - 1U) == rhs;
    };
    ASSERT_TRUE(std::equal(src.begin(), src.end(), unpacked.begin(), comparer));
  }

  // check random access
  for (uint32_t bits = 1; bits <= 32; ++bits) {
    std::vector<uint32_t> packed(packed::blocks_required_32(src.size(), bits), 0);
    packed::pack(&src[0], &src[0] + src.size(), &packed[0], bits);

    for(size_t i = 0, end = src.size(); i < end; ++i) {
      auto expected_value = src[i];
      if (bits != 32) { // 32 bit left shift cause undefined behavior
        expected_value &= ((1U << bits) - 1U);
      }

      ASSERT_EQ(expected_value, packed::at(packed.data(), i, bits));
    }
  }
}

TEST(bit_packing_tests, pack_unpack_64) {
  std::vector<uint64_t> src{
    14410, 21766, 15994, 29493, 20819, 14410123456789, 21766234567890, 159943456789012, 294934567890123, 208195678901234,
    31650, 28103, 27900, 24340, 19822, 31650123456789, 28103234567890, 279003456789012, 243404567890123, 198225678901234,
    31073, 22825, 22494,  6121, 20200, 31073123456789, 22825234567890, 224943456789012,  61214567890123, 202005678901234,
    28354, 25256,   220,     2,   393, 28354123456789, 25256234567890,   2203456789012,     24567890123,   3935678901234,
      805, 18232,   956, 21480, 20565,   805123456789, 18232234567890,   9563456789012, 214804567890123, 205655678901234,
    20500,  4324, 16372,  1064,  9878, 20500123456789,  4324234567890, 163723456789012,  10644567890123,  98785678901234,
    13639, 18301, 31582, 18341, 30711, 13639123456789, 18301234567890, 315823456789012, 183414567890123, 307115678901234,
    25801, 16556, 23070,  7921, 20539, 25801123456789, 16556234567890, 230703456789012,  79214567890123, 205395678901234,
    23571, 27043,  1344, 21307, 25545, 23571123456789, 27043234567890,  13443456789012, 213074567890123, 255455678901234,
    27844,  4796, 31011,  2238, 21070, 27844123456789,  4796234567890, 310113456789012,  22384567890123, 210705678901234,
    30090, 16475,  4683, 16839, 14253, 30090123456789, 16475234567890,  46833456789012, 168394567890123, 142535678901234,
    17744,  5467, 19431, 17022, 24011, 17744123456789,  5467234567890, 194313456789012, 170224567890123, 240115678901234,
    21970,    24, 21009, 20131,  4494, 21970123456789,    24234567890, 210093456789012, 201314567890123,  44945678901234,
     5110,  2991,  8127, 21700, 20629,  5110123456789,  2991234567890,  81273456789012, 217004567890123, 206295678901234,
    31195, 20423, 24248, 15917, 31151, 31195123456789, 20423234567890, 242483456789012, 159174567890123, 311515678901234,
     8090, 24170, 11403, 30484, 11747,  8090123456789, 24170234567890, 114033456789012, 304844567890123, 117475678901234,
    23682,  7179, 21741, 28313, 22102, 23682123456789,  7179234567890, 217413456789012, 283134567890123, 221025678901234,
    26759,  4385, 19645, 17771,  5977, 26759123456789,  4385234567890, 196453456789012, 177714567890123,  59775678901234,
    22946,  2705, 15188, 26374, 25695, 22946123456789,  2705234567890, 151883456789012, 263744567890123, 256955678901234,
    25673,  4866,  8263,  1860, 24033, 25673123456789,  4866234567890,  82633456789012,  18604567890123, 240335678901234,
    12528, 14297, 11673, 20979,   446, 12528123456789, 14297234567890, 116733456789012, 209794567890123,   4465678901234,
    31576, 18451, 13478, 11380,  6490, 31576123456789, 18451234567890, 134783456789012, 113804567890123,  64905678901234,
     2785,  3307,  1912,   382, 24139,  2785123456789,  3307234567890,  19123456789012,   3824567890123, 241395678901234,
    11483, 16582, 29873, 31287, 18465, 11483123456789, 16582234567890, 298733456789012, 312874567890123, 184655678901234,
    24084, 29421, 18341, 21654,  3290, 24084123456789, 29421234567890, 183413456789012, 216544567890123,  32905678901234,
    19579
  };

  src.resize(packed::items_required(src.size()));

  // smoke test
  {
    auto max = *std::max_element(src.begin(), src.end());
    auto bits = packed::bits_required_64(max);
    const auto expected_bits = packed::bits_required_64(*std::max_element(src.begin(), src.end()));
    ASSERT_EQ(expected_bits, bits);

    std::vector<uint64_t> packed(packed::blocks_required_64(src.size(), bits), 0);
    packed::pack(&src[0], &src[0] + src.size(), &packed[0], bits);

    std::vector<uint64_t> unpacked(src.size());
    packed::unpack(&unpacked[0], &unpacked[0] + unpacked.size(), &packed[0], bits);

    ASSERT_TRUE(std::equal(src.begin(), src.end(), unpacked.begin()));
  }
  
  // check packing [1..64]
  for (uint32_t bits = 1; bits <= 64; ++bits) {
    std::vector<uint64_t> packed(packed::blocks_required_64(src.size(), bits), 0);
    packed::pack(&src[0], &src[0] + src.size(), &packed[0], bits);

    std::vector<uint64_t> unpacked(src.size());
    packed::unpack(&unpacked[0], &unpacked[0] + unpacked.size(), &packed[0], bits);

    auto comparer = [bits] (uint64_t lhs, uint64_t rhs) {
      // 64 bit left shift cause undefined behavior
      if (64 == bits) {
        return lhs == rhs;
      }

      return (lhs & ((1ULL << bits) - 1ULL)) == rhs;
    };
    ASSERT_TRUE(std::equal(src.begin(), src.end(), unpacked.begin(), comparer));
  }
  
  // check random access
  for (uint32_t bits = 1; bits <= 64; ++bits) {
    std::vector<uint64_t> packed(packed::blocks_required_64(src.size(), bits), 0);
    packed::pack(&src[0], &src[0] + src.size(), &packed[0], bits);

    for(size_t i = 0, end = src.size(); i < end; ++i) {
      auto expected_value = src[i];
      if (bits != 64) { // 64 bit left shift cause undefined behavior
        expected_value &= ((1ULL << bits) - 1ULL);
      }

      ASSERT_EQ(expected_value, packed::at(packed.data(), i, bits));
    }
  }
}

TEST(bit_packing_tests, iterator32) {
  std::vector<uint32_t> src{
    14410, 21766, 15994, 29493, 20819,
    31650, 28103, 27900, 24340, 19822,
    31073, 22825, 22494,  6121, 20200,
    28354, 25256,   220,     2,   393,
      805, 18232,   956, 21480, 20565,
    20500,  4324, 16372,  1064,  9878,
    13639, 18301, 31582, 18341, 30711,
    25801, 16556, 23070,  7921, 20539,
    23571, 27043,  1344, 21307, 25545,
    27844,  4796, 31011,  2238, 21070,
    30090, 16475,  4683, 16839, 14253,
    17744,  5467, 19431, 17022, 24011,
    21970,    24, 21009, 20131,  4494,
     5110,  2991,  8127, 21700, 20629,
    31195, 20423, 24248, 15917, 31151,
     8090, 24170, 11403, 30484, 11747,
    23682,  7179, 21741, 28313, 22102,
    26759,  4385, 19645, 17771,  5977,
    22946,  2705, 15188, 26374, 25695,
    25673,  4866,  8263,  1860, 24033,
    12528, 14297, 11673, 20979,   446,
    31576, 18451, 13478, 11380,  6490,
     2785,  3307,  1912,   382, 24139,
    11483, 16582, 29873, 31287, 18465,
    24084, 29421, 18341, 21654,  3290,
    19579
  };

  src.resize(packed::items_required(src.size()));
  std::sort(src.begin(), src.end());

  const uint32_t max = *std::max_element(src.begin(), src.end());
  const uint32_t bits = packed::bits_required_32(max);
  const auto expected_bits = packed::bits_required_32(*std::max_element(src.begin(), src.end()));
  ASSERT_EQ(expected_bits, bits);

  std::vector<uint32_t> compressed(packed::blocks_required_32(src.size(), bits), 0);
  packed::pack(src.data(), src.data() + src.size(), compressed.data(), bits);

  packed::iterator32 begin(compressed.data(), bits);
  packed::iterator32 end(compressed.data(), bits, src.size());

  for (auto value : src) {
    const auto found = std::lower_bound(begin, end, value);
    ASSERT_NE(end, found);
  }
}
