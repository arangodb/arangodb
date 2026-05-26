#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <unordered_set>
#include <unordered_map>
#include <limits>
#include <cmath>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

static inline AqlValue makeAQLValue(int64_t x) {
  return AqlValue(arangodb::aql::AqlValueHintInt(x));
}

static inline AqlValue makeAQLValue(uint64_t x) {
  return AqlValue(arangodb::aql::AqlValueHintUInt(x));
}

static inline AqlValue makeAQLValue(double x) {
  VPackBuilder b;
  b.add(VPackValue(x));
  return AqlValue(b.slice());
}

}  // namespace

class AqlValueEqualTest : public ::testing::Test {
 protected:
  std::equal_to<AqlValue> equal;
};

class AqlValueHashEqualTest : public ::testing::Test {
 protected:
  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;
};

TEST_F(AqlValueEqualTest, equal_content_not_pointer) {
  AqlValue val1 = makeAQLValue(int64_t{42});
  AqlValue val2 = makeAQLValue(int64_t{42});
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_TRUE(equal(val2, val1));
}

TEST_F(AqlValueHashEqualTest, hash_equal_consistency_different_values) {
  AqlValue val1 = makeAQLValue(int64_t{42});
  AqlValue val2 = makeAQLValue(int64_t{43});
  EXPECT_FALSE(equal(val1, val2));
}

TEST_F(AqlValueHashEqualTest, inline_int64_same_value) {
  AqlValue val1 = makeAQLValue(int64_t{12345});
  AqlValue val2 = makeAQLValue(int64_t{12345});
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, inline_int64_different_values) {
  EXPECT_FALSE(
      equal(makeAQLValue(int64_t{12345}), makeAQLValue(int64_t{12346})));
}

TEST_F(AqlValueHashEqualTest, inline_int64_edge_cases) {
  std::vector<int64_t> vals = {0,
                               1,
                               -1,
                               std::numeric_limits<int64_t>::max(),
                               std::numeric_limits<int64_t>::min(),
                               42,
                               -42};
  for (size_t i = 0; i < vals.size(); ++i) {
    for (size_t j = 0; j < vals.size(); ++j) {
      AqlValue a = makeAQLValue(int64_t{vals[i]});
      AqlValue b = makeAQLValue(int64_t{vals[j]});
      bool eq = (vals[i] == vals[j]);
      EXPECT_EQ(eq, equal(a, b));
      if (eq) {
        EXPECT_EQ(hasher(a), hasher(b));
      }
    }
  }
}

TEST_F(AqlValueHashEqualTest, inline_uint64_same_value) {
  AqlValue val1 = makeAQLValue(uint64_t{12345ULL});
  AqlValue val2 = makeAQLValue(uint64_t{12345ULL});
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, inline_uint64_different_values) {
  EXPECT_FALSE(equal(makeAQLValue(uint64_t{12345ULL}),
                     makeAQLValue(uint64_t{12346ULL})));
}

TEST_F(AqlValueHashEqualTest, inline_uint64_large_value) {
  uint64_t large = (1ULL << 63) + 100;
  AqlValue val1 = makeAQLValue(uint64_t{large});
  AqlValue val2 = makeAQLValue(uint64_t{large});
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, inline_double_same_value) {
  AqlValue val1 = makeAQLValue(3.14159);
  AqlValue val2 = makeAQLValue(3.14159);
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, inline_double_different_values) {
  EXPECT_FALSE(equal(makeAQLValue(3.14159), makeAQLValue(3.14160)));
}

TEST_F(AqlValueHashEqualTest, inline_double_zero_variants) {
  // -0.0 and +0.0 must be equal and hash identically — both are normalized
  // to 0.0 before hashing so the hash contract holds.
  AqlValue pos = makeAQLValue(0.0);
  AqlValue neg = makeAQLValue(-0.0);
  EXPECT_TRUE(equal(pos, neg));
  EXPECT_EQ(hasher(pos), hasher(neg));
}

TEST_F(AqlValueHashEqualTest, range_same_value) {
  AqlValue val1(1, 100);
  AqlValue val2(1, 100);
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
  val1.destroy();
  val2.destroy();
}

TEST_F(AqlValueHashEqualTest, range_different_values) {
  AqlValue val1(1, 100);
  AqlValue val2(1, 101);
  AqlValue val3(2, 100);
  EXPECT_FALSE(equal(val1, val2));
  EXPECT_FALSE(equal(val1, val3));
  val1.destroy();
  val2.destroy();
  val3.destroy();
}

TEST_F(AqlValueHashEqualTest, string_same_value) {
  AqlValue val1("hello");
  AqlValue val2("hello");
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, string_different_values) {
  EXPECT_FALSE(equal(AqlValue("hello"), AqlValue("world")));
}

TEST_F(AqlValueHashEqualTest, string_empty) {
  AqlValue val1("");
  AqlValue val2("");
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, string_unicode) {
  AqlValue val1("café");
  AqlValue val2("café");
  AqlValue val3("cafe");
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
  EXPECT_FALSE(equal(val1, val3));
}

TEST_F(AqlValueHashEqualTest, string_long_same_value) {
  std::string longStr(200, 'x');
  AqlValue val1(longStr);
  AqlValue val2(longStr);
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
  val1.destroy();
  val2.destroy();
}

TEST_F(AqlValueHashEqualTest, array_same_value) {
  VPackBuilder b1, b2;
  b1.openArray();
  b1.add(VPackValue(1));
  b1.add(VPackValue(2));
  b1.close();
  b2.openArray();
  b2.add(VPackValue(1));
  b2.add(VPackValue(2));
  b2.close();
  AqlValue val1(b1.slice());
  AqlValue val2(b2.slice());
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, array_different_values) {
  VPackBuilder b1, b2;
  b1.openArray();
  b1.add(VPackValue(1));
  b1.add(VPackValue(2));
  b1.close();
  b2.openArray();
  b2.add(VPackValue(1));
  b2.add(VPackValue(3));
  b2.close();
  AqlValue val1(b1.slice());
  AqlValue val2(b2.slice());
  EXPECT_FALSE(equal(val1, val2));
}

TEST_F(AqlValueHashEqualTest, object_same_value) {
  VPackBuilder b1, b2;
  b1.openObject();
  b1.add("key1", VPackValue("value1"));
  b1.add("key2", VPackValue(42));
  b1.close();
  b2.openObject();
  b2.add("key1", VPackValue("value1"));
  b2.add("key2", VPackValue(42));
  b2.close();
  AqlValue val1(b1.slice());
  AqlValue val2(b2.slice());
  // Longer keys/values push this past 16 bytes → VPACK_MANAGED_SLICE.
  ASSERT_TRUE(val1.requiresDestruction());
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
  val1.destroy();
  val2.destroy();
}

TEST_F(AqlValueHashEqualTest, null_values) {
  AqlValue val1{arangodb::aql::AqlValueHintNull{}};
  AqlValue val2{arangodb::aql::AqlValueHintNull{}};
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, none_values) {
  AqlValue val1{arangodb::aql::AqlValueHintNone{}};
  AqlValue val2{arangodb::aql::AqlValueHintNone{}};
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, boolean_values) {
  AqlValue t1(arangodb::aql::AqlValueHintBool(true));
  AqlValue t2(arangodb::aql::AqlValueHintBool(true));
  AqlValue f1(arangodb::aql::AqlValueHintBool(false));
  EXPECT_TRUE(equal(t1, t2));
  EXPECT_EQ(hasher(t1), hasher(t2));
  EXPECT_FALSE(equal(t1, f1));
}

TEST_F(AqlValueHashEqualTest, number_semantic_equality_int64_vs_vpack) {
  AqlValue val1 = makeAQLValue(int64_t{42});
  VPackBuilder b;
  b.add(VPackValue(42));
  AqlValue val2(b.slice());
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, number_semantic_equality_uint64_vs_vpack) {
  uint64_t large = (1ULL << 63) + 100;
  AqlValue val1 = makeAQLValue(uint64_t{large});
  VPackBuilder b;
  b.add(VPackValue(large));
  AqlValue val2(b.slice());
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, number_semantic_equality_double_vs_vpack) {
  AqlValue val1 = makeAQLValue(3.14159);
  VPackBuilder b;
  b.add(VPackValue(3.14159));
  AqlValue val2(b.slice());
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, number_semantic_equality_int_vs_double) {
  // Integer 42 and double 42.0 are equal under normalizedHash.
  AqlValue val1 = makeAQLValue(int64_t{42});
  VPackBuilder b;
  b.add(VPackValue(42.0));
  AqlValue val2(b.slice());
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, cross_type_range_vs_number) {
  AqlValue rangeVal(1, 100);
  VPackBuilder b;
  b.add(VPackValue(42));
  AqlValue numVal(b.slice());
  EXPECT_FALSE(equal(rangeVal, numVal));
  EXPECT_FALSE(equal(numVal, rangeVal));
  rangeVal.destroy();
}

TEST_F(AqlValueHashEqualTest, unordered_set_deduplication) {
  std::unordered_set<AqlValue> set;
  AqlValue val1 = makeAQLValue(int64_t{42});
  AqlValue val2 = makeAQLValue(int64_t{42});
  set.insert(val1);
  EXPECT_EQ(1U, set.size());
  EXPECT_FALSE(set.insert(val2).second);
  EXPECT_EQ(1U, set.size());

  // 42.0 as VPACK_INLINE_DOUBLE — different storage type, same numeric value.
  VPackBuilder b;
  b.add(VPackValue(42.0));
  AqlValue val3(b.slice());
  ASSERT_EQ(AqlValue::AqlValueType::VPACK_INLINE_DOUBLE, val3.type());
  EXPECT_FALSE(set.insert(val3).second);
  EXPECT_EQ(1U, set.size());
}

TEST_F(AqlValueHashEqualTest, unordered_map_key_lookup) {
  std::unordered_map<AqlValue, int> map;
  AqlValue key1 = makeAQLValue(int64_t{42});
  map[key1] = 100;

  AqlValue key2 = makeAQLValue(int64_t{42});
  EXPECT_EQ(100, map[key2]);

  VPackBuilder b;
  b.add(VPackValue(42.0));
  AqlValue key3(b.slice());  // VPACK_INLINE_DOUBLE, same numeric value as key1
  ASSERT_EQ(AqlValue::AqlValueType::VPACK_INLINE_DOUBLE, key3.type());
  EXPECT_EQ(100, map[key3]);
}

TEST_F(AqlValueHashEqualTest, edge_case_large_numbers) {
  auto check = [&](auto val) {
    auto a = makeAQLValue(val);
    auto b = makeAQLValue(val);
    EXPECT_TRUE(equal(a, b));
    EXPECT_EQ(hasher(a), hasher(b));
  };
  check(std::numeric_limits<int64_t>::max());
  check(std::numeric_limits<int64_t>::min());
  check(std::numeric_limits<uint64_t>::max());
}

TEST_F(AqlValueHashEqualTest, stress_test_distinct_values) {
  // 2001 distinct integers should produce 2001 distinct set entries.
  std::unordered_set<AqlValue> set;
  for (int i = -1000; i <= 1000; ++i) {
    set.insert(makeAQLValue(int64_t{i}));
  }
  EXPECT_EQ(2001U, set.size());
}
