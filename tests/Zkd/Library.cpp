////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include <array>
#include <iostream>
#include <utility>
#include <vector>

#include "Zkd/ZkdHelper.h"

#include <rocksdb/comparator.h>
#include <rocksdb/slice.h>

using namespace arangodb;

static std::ostream& operator<<(std::ostream& os, std::vector<zkd::byte_string> const& bsvec) {
  os << "{";
  if (!bsvec.empty()) {
    auto it = bsvec.begin();
    os << *it;
    for (++it; it != bsvec.end(); ++it) {
      os << ", ";
      os << *it;
    }
  }
  os << "}";

  return os;
}

static std::ostream& operator<<(std::ostream& os,
                                std::vector<zkd::CompareResult> const& cmpResult) {
  os << "{";
  if (!cmpResult.empty()) {
    auto it = cmpResult.begin();
    os << *it;
    for (++it; it != cmpResult.end(); ++it) {
      os << ", ";
      os << *it;
    }
  }
  os << "}";

  return os;
}

#include "gtest/gtest.h"

using namespace zkd;

TEST(Zkd_byteStringLiteral, bs) {
  EXPECT_THROW(""_bs, std::invalid_argument);
  EXPECT_THROW(" "_bs, std::invalid_argument);
  EXPECT_THROW("'"_bs, std::invalid_argument);
  EXPECT_THROW("2"_bs, std::invalid_argument);
  EXPECT_THROW("a"_bs, std::invalid_argument);
  EXPECT_THROW("\0"_bs, std::invalid_argument);
  EXPECT_THROW("02"_bs, std::invalid_argument);
  EXPECT_THROW("12"_bs, std::invalid_argument);
  EXPECT_THROW("0 2"_bs, std::invalid_argument);
  EXPECT_THROW("1 2"_bs, std::invalid_argument);

  EXPECT_EQ(byte_string{std::byte{0x00}}, "0"_bs);
  EXPECT_EQ(byte_string{std::byte{0x80}}, "1"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{0x00}}), "00000000'0"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{0x80}}), "00000000'1"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{0x80}}), "0'00000001"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{0x80}}), "0 00000001"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{0x80}}), "0 000 000 01"_bs);
  EXPECT_EQ((byte_string{std::byte{0x01}, std::byte{0x00}}), "00000001'0"_bs);
  EXPECT_EQ((byte_string{std::byte{0x01}, std::byte{0x00}}), "0'00000010"_bs);
  EXPECT_EQ((byte_string{std::byte{0x80}, std::byte{0x00}}), "1'00000000"_bs);
  EXPECT_EQ((byte_string{std::byte{0xa8}, std::byte{0xa8}}),
            "10101000'101010"_bs);
  EXPECT_EQ((byte_string{std::byte{0x15}, std::byte{0x15}, std::byte{0}}),
            "00010101'00010101'0"_bs);

  EXPECT_EQ(byte_string{std::byte{0x00}}, "00000000"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{0x00}}),
            "00000000 00000000"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{1}}),
            "00000000 00000001"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{2}}),
            "00000000 00000010"_bs);
  EXPECT_EQ((byte_string{std::byte{1}, std::byte{0x00}}),
            "00000001 00000000"_bs);
  EXPECT_EQ((byte_string{std::byte{42}, std::byte{42}}), "00101010 00101010"_bs);
  EXPECT_EQ((byte_string{std::byte{0x00}, std::byte{42}, std::byte{42}}),
            "00000000 00101010 00101010"_bs);
}

TEST(Zkd_interleave, d0) {
  auto res = interleave({});
  ASSERT_EQ(byte_string{}, res);
}

TEST(Zkd_interleave, d1_empty) {
  auto res = interleave({{}});
  ASSERT_EQ(byte_string{}, res);
}

TEST(Zkd_interleave, d1_multi) {
  auto const testees = {
      "00101010"_bs,
      "00101010'00101010"_bs,
      "00000001'00000010'00000011"_bs,
  };
  for (auto const& testee : testees) {
    auto const res = interleave({testee});
    EXPECT_EQ(testee, res);
  }
}

TEST(Zkd_interleave, d2_empty) {
  auto res = interleave({{}, {}});
  ASSERT_EQ(byte_string{}, res);
}

TEST(Zkd_interleave, d2_multi) {
  auto const testees = {
      std::pair{"01010101'10101010"_bs,
                std::tuple{"00001111"_bs, "11110000"_bs}},
      {"01010101'01010101'00110011'00110011"_bs,
       {"00000000'01010101"_bs, "11111111'01010101"_bs}},
      {"10101010'10101010'01010101'01010101"_bs,
       {"11111111"_bs, "00000000'11111111"_bs}},
      {"01010111'01010111'00010001'00010001'01000100'01000100"_bs,
       {"00010001"_bs, "11111111'01010101'10101010"_bs}},
  };
  for (auto const& it : testees) {
    auto const& [expected, testee] = it;
    auto const res = interleave({std::get<0>(testee), std::get<1>(testee)});
    EXPECT_EQ(expected, res);
  }
}

TEST(Zkd_transpose, d3_empty) {
  auto res = transpose({}, 3);
  ASSERT_EQ(res, (std::vector<byte_string>{{}, {}, {}}));
}

TEST(Zkd_transpose, d3_multi) {
  auto const testees = {
      std::pair{"00011100"_bs,
                std::vector{"01000000"_bs, "01000000"_bs, "01000000"_bs}},
      {"00011110"_bs, {"01100000"_bs, "01000000"_bs, "01000000"_bs}},
      {"10101010"_bs, {"10100000"_bs, "01000000"_bs, "10000000"_bs}},
  };
  for (auto const& it : testees) {
    auto const& [testee, expected] = it;
    auto res = transpose(testee, 3);
    ASSERT_EQ(res, expected);
  }
}

TEST(Zkd_compareBox, d2_eq) {
  auto min_v = interleave({"00000101"_bs, "01001101"_bs});  // 00 01 00 00 01 11 00 11
  auto max_v = interleave({"00100011"_bs, "01111001"_bs});  // 00 01 11 01 01 00 10 11
  auto v = interleave({"00001111"_bs, "01010110"_bs});  // 00 01 00 01 10 11 11 10

  auto res = compareWithBox(v, min_v, max_v, 2);

  // 00 01 00 00 01 11 00 11 -- min (5, 77)
  // 00 01 00 01 10 11 11 10 -- cur (15, 86)
  // 00 01 11 01 01 00 10 11 -- max (35, 121)

  EXPECT_EQ(res[0].flag, 0);
  EXPECT_EQ(res[0].saveMin, 4);
  EXPECT_EQ(res[0].saveMax, 2);
  EXPECT_EQ(res[0].outStep, CompareResult::max);
  EXPECT_EQ(res[1].flag, 0);
  EXPECT_EQ(res[1].saveMin, 3);
  EXPECT_EQ(res[1].saveMax, 2);
  EXPECT_EQ(res[1].outStep, CompareResult::max);
}

TEST(Zkd_compareBox, d2_eq2) {
  auto min_v = interleave({"00000010"_bs, "00000011"_bs});  // 00 00 00 00 00 00 11 01
  auto max_v = interleave({"00000110"_bs, "00000101"_bs});  // 00 00 00 00 00 11 10 01
  auto v = interleave({"00000011"_bs, "00000011"_bs});  // 00 00 00 00 00 00 11 11

  auto res = compareWithBox(v, min_v, max_v, 2);

  EXPECT_EQ(res[0].flag, 0);
  EXPECT_EQ(res[0].saveMin, 7);
  EXPECT_EQ(res[0].saveMax, 5);
  EXPECT_EQ(res[0].outStep, CompareResult::max);
  EXPECT_EQ(res[1].flag, 0);
  EXPECT_EQ(res[1].saveMin, CompareResult::max);
  EXPECT_EQ(res[1].saveMax, 5);
  EXPECT_EQ(res[1].outStep, CompareResult::max);
}

TEST(Zkd_compareBox, d2_less) {
  auto min_v = interleave({"00000101"_bs, "01001101"_bs});
  auto max_v = interleave({"00100011"_bs, "01111001"_bs});
  auto v = interleave({"00000011"_bs, "01010110"_bs});

  auto res = compareWithBox(v, min_v, max_v, 2);

  EXPECT_EQ(res[0].flag, -1);
  EXPECT_EQ(res[0].saveMin, CompareResult::max);
  EXPECT_EQ(res[0].saveMax, 2);
  EXPECT_EQ(res[0].outStep, 5);
  EXPECT_EQ(res[1].flag, 0);
  EXPECT_EQ(res[1].saveMin, 3);
  EXPECT_EQ(res[1].saveMax, 2);
  EXPECT_EQ(res[1].outStep, CompareResult::max);
}

TEST(Zkd_compareBox, d2_x_less_y_greater) {
  auto min_v = interleave({"00000100"_bs, "00000010"_bs});  // 00 00 00 00 00 10 01 00
  auto max_v = interleave({"00001000"_bs, "00000110"_bs});  // 00 00 00 00 10 01 01 00
  auto v = interleave({"00000011"_bs, "00010000"_bs});  // 00 00 00 01 00 00 10 10

  auto res = compareWithBox(v, min_v, max_v, 2);

  EXPECT_EQ(res[0].flag, -1);
  EXPECT_EQ(res[0].saveMin, -1);
  EXPECT_EQ(res[0].saveMax, 4);
  EXPECT_EQ(res[0].outStep, 5);
  EXPECT_EQ(res[1].flag, 1);
  EXPECT_EQ(res[1].saveMin, 3);
  EXPECT_EQ(res[1].saveMax, -1);
  EXPECT_EQ(res[1].outStep, 3);
}

TEST(Zkd_compareBox, d3_x_less_y_greater_z_eq) {
  auto min_v = interleave({"00000100"_bs, "00000010"_bs, "00000000"_bs});  // 000 000 000 000 000 100 010 000
  auto max_v = interleave({"00001000"_bs, "00000110"_bs, "00000010"_bs});  // 000 000 000 000 100 010 011 000
  auto v = interleave({"00000011"_bs, "00010000"_bs, "00000010"_bs});  // 000 000 000 010 000 000 101 100

  auto res = compareWithBox(v, min_v, max_v, 3);

  EXPECT_EQ(res[0].flag, -1);
  EXPECT_EQ(res[0].saveMin, -1);
  EXPECT_EQ(res[0].saveMax, 4);
  EXPECT_EQ(res[0].outStep, 5);
  EXPECT_EQ(res[1].flag, 1);
  EXPECT_EQ(res[1].saveMin, 3);
  EXPECT_EQ(res[1].saveMax, -1);
  EXPECT_EQ(res[1].outStep, 3);
  EXPECT_EQ(res[2].flag, 0);
  EXPECT_EQ(res[2].saveMin, 6);
  EXPECT_EQ(res[2].saveMax, -1);
  EXPECT_EQ(res[2].outStep, -1);
}

TEST(Zkd_compareBox, testFigure41_3) {
  // lower point of the box: (2, 2)
  auto min_v = interleave({"00000010"_bs, "00000010"_bs});
  // upper point of the box: (5, 4)
  auto max_v = interleave({"00000101"_bs, "00000100"_bs});

  auto v = interleave({"00000110"_bs, "00000010"_bs});  // (6, 2)
  auto res = compareWithBox(v, min_v, max_v, 2);

  EXPECT_EQ(res[0].flag, 1);
  EXPECT_EQ(res[0].saveMin, 5);
  EXPECT_EQ(res[0].saveMax, CompareResult::max);
  EXPECT_EQ(res[0].outStep, 6);
  EXPECT_EQ(res[1].flag, 0);
  EXPECT_EQ(res[1].saveMin, CompareResult::max);
  EXPECT_EQ(res[1].saveMax, 5);
  EXPECT_EQ(res[1].outStep, CompareResult::max);
}

TEST(Zkd_rocksdb, convert_bytestring) {
  auto const data = {
      "00011100"_bs,
      "11111111'01010101"_bs,
  };

  for (auto const& it : data) {
    auto const slice =
        rocksdb::Slice(reinterpret_cast<char const*>(it.c_str()), it.size());
    auto const string =
        byte_string{reinterpret_cast<std::byte const*>(slice.data()), slice.size()};
    EXPECT_EQ(it, string);
    EXPECT_EQ(it.size(), slice.size());
    EXPECT_EQ(it.size(), string.size());
    EXPECT_EQ(0, memcmp(it.data(), slice.data(), it.size()));
  }
}

static auto sliceFromString(byte_string const& str) -> rocksdb::Slice {
  return rocksdb::Slice(reinterpret_cast<char const*>(str.c_str()), str.size());
}

auto viewFromSlice(rocksdb::Slice slice) -> byte_string_view {
  return byte_string_view{reinterpret_cast<std::byte const*>(slice.data()),
                          slice.size()};
}

TEST(Zkd_rocksdb, cmp_slice) {
  enum class Cmp : int { LT = -1, EQ = 0, GT = 1 };
  auto const data = {
      std::pair{Cmp::EQ, std::pair{"00101010"_bs, "00101010"_bs}},
      std::pair{Cmp::EQ,
                std::pair{"00000001'00000010"_bs, "00000001'00000010"_bs}},
      std::pair{Cmp::LT,
                std::pair{"00000001'00000001"_bs, "00000001'00000010"_bs}},
      std::pair{Cmp::GT, std::pair{"10000000"_bs, "01111111'11111111"_bs}},
      // TODO more tests
  };

  auto const* const cmp = rocksdb::BytewiseComparator();

  for (auto const& it : data) {
    auto const& [expected, testee] = it;
    auto const& [left, right] = testee;
    EXPECT_EQ(expected == Cmp::LT, cmp->Compare(sliceFromString(left), sliceFromString(right)) < 0)
        << "left = " << left << ", right = " << right;
    EXPECT_EQ(expected == Cmp::EQ, cmp->Compare(sliceFromString(left), sliceFromString(right)) == 0)
        << "left = " << left << ", right = " << right;
    EXPECT_EQ(expected == Cmp::EQ, cmp->Equal(sliceFromString(left), sliceFromString(right)))
        << "left = " << left << ", right = " << right;
    EXPECT_EQ(expected == Cmp::GT, cmp->Compare(sliceFromString(left), sliceFromString(right)) > 0)
        << "left = " << left << ", right = " << right;
  }
}

TEST(Zkd_getNextZValue, testFigure41) {
  // lower point of the box: (2, 2)
  auto const pMin = interleave({"00000010"_bs, "00000010"_bs});
  // upper point of the box: (4, 5)
  auto const pMax = interleave(std::vector{"00000100"_bs, "00000101"_bs});

  auto test = [&pMin, &pMax](std::vector<byte_string> const& inputCoords,
                             std::optional<std::vector<zkd::byte_string>> const& expectedCoords) {
    auto const input = interleave(inputCoords);
    auto const expected = std::invoke([&]() -> std::optional<byte_string> {
      if (expectedCoords.has_value()) {
        return interleave(expectedCoords.value());
      } else {
        return std::nullopt;
      }
    });
    auto cmpResult = compareWithBox(input, pMin, pMax, 2);
    // input should be outside the box:
    auto sstr = std::stringstream{};
    if (expectedCoords.has_value()) {
      sstr << expectedCoords.value();
    } else {
      sstr << "n/a";
    }
    ASSERT_TRUE(std::any_of(cmpResult.begin(), cmpResult.end(),
                            [](auto const& it) { return it.flag != 0; }))
        << "with input=" << inputCoords << ", expected=" << sstr.str()
        << ", result=" << cmpResult;
    auto result = getNextZValue(input, pMin, pMax, cmpResult);
    auto sstr2 = std::stringstream{};
    if (result.has_value()) {
      sstr2 << result.value() << "/" << transpose(result.value(), 2);
    } else {
      sstr2 << "n/a";
    }
    EXPECT_EQ(expected, result)
        << "with input=" << inputCoords << ", expected=" << sstr.str()
        << ", result=" << sstr2.str() << ", cmpResult=" << cmpResult;
    // TODO should cmpResult be checked?
  };

  // z-curve inside the box [ (2, 2); (4, 5) ] goes through the following
  // points. the value after -/> is outside the box. The next line continues
  // with the next point on the curve inside the box. (2, 2) -> (2, 3) -> (3, 2)
  // -> (3, 3) -/> (0, 4) (2, 4) -> (3, 4) -> (2, 5) -> (3, 5) -/> (2, 6) (4, 2)
  // -> (4, 3) -/> (5, 2) (4, 4) -> (4, 5) -/> (5, 4)

  test({"00000000"_bs, "00000000"_bs}, {{"00000010"_bs, "00000010"_bs}});
  test({"00000000"_bs, "00000100"_bs}, {{"00000010"_bs, "00000100"_bs}});
  test({"00000010"_bs, "00000110"_bs}, {{"00000100"_bs, "00000010"_bs}});
  test({"00000101"_bs, "00000010"_bs}, {{"00000100"_bs, "00000100"_bs}});
  test({"00000101"_bs, "00000100"_bs}, std::nullopt);

  for (uint8_t xi = 0; xi < 8; ++xi) {
    for (uint8_t yi = 0; yi < 8; ++yi) {
      bool const inBox = 2 <= xi && xi <= 4 && 2 <= yi && yi <= 5;
      auto const input = interleave({{std::byte{xi}}, {std::byte{yi}}});

      auto cmpResult = compareWithBox(input, pMin, pMax, 2);
      // assert that compareWithBox agrees with our `inBox` bool
      ASSERT_EQ(inBox, std::all_of(cmpResult.begin(), cmpResult.end(),
                                   [](auto const& it) { return it.flag == 0; }))
          << "xi=" << int(xi) << ", yi=" << int(yi) << ", cmpResult=" << cmpResult;
      if (!inBox) {
        auto result = getNextZValue(input, pMin, pMax, cmpResult);
        if (result.has_value()) {
          // TODO make the check more precise, check that it's the correct point
          auto res = compareWithBox(result.value(), pMin, pMax, 2);
          EXPECT_TRUE(std::all_of(res.begin(), res.end(),
                                  [](auto const& it) { return it.flag == 0; }));
        } else {
        }
        // TODO add a check for the else branch, whether it's correct to not
        //      return a value.
      }
    }
  }
}

TEST(Zkd_testInBox, regression_1) {
  auto cur = zkd::interleave(
      {byte_string{0x5f_b, 0xf8_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b},
       byte_string{0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b}});
  auto const& min = cur;
  auto max = zkd::interleave(
      {byte_string{0x60_b, 0x04_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b},
       byte_string{0x80_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b}});

  ASSERT_TRUE(testInBox(cur, min, max, 2));
}
