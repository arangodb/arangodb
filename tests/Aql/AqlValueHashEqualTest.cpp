////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/AqlValue.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

constexpr std::string_view kLongObject =
    R"({"thisField": "is too large to be inlined, deliberately so"})";

AqlValue fromJson(std::string_view json) {
  return AqlValue{VPackParser::fromJson(json)->slice()};
}

AqlValue makeDouble(double d) {
  VPackBuilder b;
  b.add(VPackValue(d));
  return AqlValue{b.slice()};
}

}  // namespace

class AqlValueHashEqualTest : public ::testing::Test {
 protected:
  void expectEqual(AqlValue const& a, AqlValue const& b) {
    EXPECT_TRUE(equal(a, b));
    EXPECT_TRUE(equal(b, a));
    EXPECT_EQ(hasher(a), hasher(b));
  }

  void expectDistinct(AqlValue const& a, AqlValue const& b) {
    EXPECT_FALSE(equal(a, b));
    EXPECT_FALSE(equal(b, a));
  }

  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;
};

TEST_F(AqlValueHashEqualTest, managed_slices_compare_by_content) {
  for (auto make : {+[] { return fromJson(kLongObject); },
                    +[] { return AqlValue{std::string(200, 'x')}; }}) {
    AqlValue a = make();
    AqlValue b = make();
    ASSERT_EQ(AqlValue::VPACK_MANAGED_SLICE, a.type());
    ASSERT_NE(a.data(), b.data());
    expectEqual(a, b);
    a.destroy();
    b.destroy();
  }

  AqlValue a{std::string(200, 'x')};
  AqlValue b{std::string(200, 'y')};
  expectDistinct(a, b);
  a.destroy();
  b.destroy();
}

TEST_F(AqlValueHashEqualTest, slice_pointer_vs_managed_slice_same_content) {
  auto builder = VPackParser::fromJson(kLongObject);
  AqlValue managed = fromJson(kLongObject);
  ASSERT_EQ(AqlValue::VPACK_MANAGED_SLICE, managed.type());
  AqlValue pointer{builder->slice().start()};
  ASSERT_EQ(AqlValue::VPACK_SLICE_POINTER, pointer.type());
  expectEqual(managed, pointer);
  managed.destroy();
}

TEST_F(AqlValueHashEqualTest, managed_string_vs_managed_slice_same_content) {
  // clones of a MANAGED_STRING are MANAGED_SLICEs
  auto builder = VPackParser::fromJson(kLongObject);
  auto bytes = builder->slice();
  auto makeString = [&] {
    DocumentData doc =
        std::make_unique<std::string>(bytes.startAs<char>(), bytes.byteSize());
    return AqlValue{doc};
  };

  AqlValue str1 = makeString();
  AqlValue str2 = makeString();
  AqlValue managed{bytes};
  ASSERT_EQ(AqlValue::VPACK_MANAGED_STRING, str1.type());
  ASSERT_EQ(AqlValue::VPACK_MANAGED_SLICE, managed.type());
  expectEqual(str1, str2);
  expectEqual(str1, managed);
  str1.destroy();
  str2.destroy();
  managed.destroy();
}

TEST_F(AqlValueHashEqualTest, range_compares_by_bounds) {
  AqlValue a{1, 100};
  AqlValue b{1, 100};
  AqlValue otherHigh{1, 101};
  AqlValue otherLow{2, 100};
  ASSERT_NE(a.data(), b.data());
  expectEqual(a, b);
  expectDistinct(a, otherHigh);
  expectDistinct(a, otherLow);
  a.destroy();
  b.destroy();
  otherHigh.destroy();
  otherLow.destroy();
}

TEST_F(AqlValueHashEqualTest, range_never_equals_non_range) {
  AqlValue range{1, 3};
  AqlValue array = fromJson("[1,2,3]");
  AqlValue number{AqlValueHintInt{1}};
  AqlValue managed = fromJson(kLongObject);
  expectDistinct(range, array);
  expectDistinct(range, number);
  expectDistinct(range, managed);
  range.destroy();
  array.destroy();
  managed.destroy();
}

TEST_F(AqlValueHashEqualTest, aql_equal_values_stay_distinct) {
  // AQL comparison pads missing elements/attributes with null
  auto check = [&](std::string_view left, std::string_view right) {
    SCOPED_TRACE(std::string{left} + " vs " + std::string{right});
    AqlValue a = fromJson(left);
    AqlValue b = fromJson(right);
    expectDistinct(a, b);
    a.destroy();
    b.destroy();
  };
  check("[]", "[null]");
  check("{}", R"({"a":null})");
  check(R"({"a":1,"b":2})", R"({"b":2,"a":1})");
}

TEST_F(AqlValueHashEqualTest, int_encodings_are_equal) {
  VPackBuilder small;
  small.add(VPackValue(5));
  expectEqual(AqlValue{AqlValueHintInt{5}}, AqlValue{small.slice()});

  VPackBuilder negative;
  negative.add(VPackValue(-42));
  expectEqual(AqlValue{AqlValueHintInt{-42}}, AqlValue{negative.slice()});

  uint64_t const large = std::numeric_limits<uint64_t>::max();
  VPackBuilder unsignedLarge;
  unsignedLarge.add(VPackValue(large));
  expectEqual(AqlValue{AqlValueHintUInt{large}},
              AqlValue{unsignedLarge.slice()});
}

TEST_F(AqlValueHashEqualTest, int_and_double_stay_distinct) {
  // merging would turn the double into an int on the wire
  expectDistinct(AqlValue{AqlValueHintInt{42}}, makeDouble(42.0));
}

TEST_F(AqlValueHashEqualTest, negative_zero_and_zero_stay_distinct) {
  // merging would flip the sign on the wire
  AqlValue pos = makeDouble(0.0);
  AqlValue neg = makeDouble(-0.0);
  expectDistinct(pos, neg);
  EXPECT_EQ(pos.hash(), neg.hash());
}

TEST_F(AqlValueHashEqualTest, nan_equals_nan) {
  // with IEEE NaN != NaN, a set would never find it again
  expectEqual(makeDouble(std::numeric_limits<double>::quiet_NaN()),
              makeDouble(std::numeric_limits<double>::quiet_NaN()));
}

TEST_F(AqlValueHashEqualTest, unordered_set_dedups_by_content) {
  AqlValue a = fromJson(kLongObject);
  AqlValue b = fromJson(kLongObject);
  AqlValue rangeA{1, 100};
  AqlValue rangeB{1, 100};

  std::unordered_set<AqlValue> set;
  EXPECT_TRUE(set.insert(a).second);
  EXPECT_FALSE(set.insert(b).second);
  EXPECT_TRUE(set.insert(rangeA).second);
  EXPECT_FALSE(set.insert(rangeB).second);
  EXPECT_EQ(2U, set.size());

  a.destroy();
  b.destroy();
  rangeA.destroy();
  rangeB.destroy();
}
