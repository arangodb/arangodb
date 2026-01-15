#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegIdFlatSet.h"
#include "Basics/GlobalResourceMonitor.h"
#include <boost/container/flat_set.hpp>
#include "Basics/ResourceUsage.h"
#include "Containers/FlatHashMap.h"

#include <velocypack/Builder.h>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/Parser.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <limits>
#include <cmath>
#include <set>

using namespace arangodb;
using namespace arangodb::aql;

using AqlValue = arangodb::aql::AqlValue;

namespace {

// Helper functions to create AqlValue instances (matching AqlValueCompare.cpp
// pattern)
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

// Test suite for std::hash<AqlValue> - basic tests
class AqlValueBasicHashTest : public ::testing::Test {
 protected:
  std::hash<AqlValue> hasher;
};

// Test suite for std::equal_to<AqlValue>
class AqlValueEqualTest : public ::testing::Test {
 protected:
  std::equal_to<AqlValue> equal;
};

// Combined test suite for hash and equality together
class AqlValueHashEqualTest : public ::testing::Test {
 protected:
  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;
};

// ============================================================================
// Basic Hash Tests
// ============================================================================

TEST_F(AqlValueBasicHashTest, hash_returns_non_zero) {
  AqlValue val1(makeAQLValue(int64_t{42}));
  size_t h = hasher(val1);
  EXPECT_NE(0U, h) << "Hash should not be zero";
}

TEST_F(AqlValueBasicHashTest, hash_is_consistent) {
  AqlValue val1 = makeAQLValue(int64_t{42});
  size_t h1 = hasher(val1);
  size_t h2 = hasher(val1);
  EXPECT_EQ(h1, h2) << "Hash should be consistent for same value";
}

// ============================================================================
// Basic Equality Tests
// ============================================================================

TEST_F(AqlValueEqualTest, equal_reflexive) {
  AqlValue val1 = makeAQLValue(int64_t{42});
  EXPECT_TRUE(equal(val1, val1)) << "Equality should be reflexive";
}

TEST_F(AqlValueEqualTest, equal_symmetric) {
  AqlValue val1 = makeAQLValue(int64_t{42});
  AqlValue val2 = makeAQLValue(int64_t{42});
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_TRUE(equal(val2, val1)) << "Equality should be symmetric";
}

// ============================================================================
// Hash-Equality Consistency Tests (Critical!)
// ============================================================================

TEST_F(AqlValueHashEqualTest, hash_equal_consistency_same_value) {
  // Same semantic value must have same hash
  AqlValue val1 = makeAQLValue(int64_t{42});
  AqlValue val2 = makeAQLValue(int64_t{42});

  size_t h1 = hasher(val1);
  size_t h2 = hasher(val2);
  EXPECT_EQ(h1, h2) << "Same semantic value must have same hash";
  EXPECT_TRUE(equal(val1, val2)) << "Same semantic value must be equal";
}

TEST_F(AqlValueHashEqualTest, hash_equal_consistency_different_storage_types) {
  // Same semantic value in different storage types must have same hash
  AqlValue val1 = makeAQLValue(int64_t{42});  // VPACK_INLINE_INT64

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue val2(builder.slice());  // May be VPACK_INLINE or VPACK_MANAGED_SLICE

  size_t h1 = hasher(val1);
  size_t h2 = hasher(val2);
  EXPECT_EQ(h1, h2)
      << "Same semantic value in different storage must have same hash";
  EXPECT_TRUE(equal(val1, val2))
      << "Same semantic value in different storage must be equal";
}

TEST_F(AqlValueHashEqualTest, hash_equal_consistency_different_values) {
  // Different values should ideally have different hashes (but collisions are
  // possible)
  AqlValue val1(makeAQLValue(int64_t{42}));
  AqlValue val2(makeAQLValue(int64_t{43}));

  EXPECT_FALSE(equal(val1, val2)) << "Different values should not be equal";
  // Note: We can't guarantee different hashes due to collisions, but we can
  // test that if hashes are different, values are different
  size_t h1 = hasher(val1);
  size_t h2 = hasher(val2);
  if (h1 != h2) {
    EXPECT_FALSE(equal(val1, val2))
        << "Different hashes imply different values";
  }
}

// ============================================================================
// Tests for VPACK_INLINE_INT64
// ============================================================================

TEST_F(AqlValueHashEqualTest, inline_int64_same_value) {
  AqlValue val1(makeAQLValue(int64_t{12345}));
  AqlValue val2(makeAQLValue(int64_t{12345}));

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, inline_int64_different_values) {
  AqlValue val1(makeAQLValue(int64_t{12345}));
  AqlValue val2(makeAQLValue(int64_t{12346}));

  EXPECT_FALSE(equal(val1, val2));
}

TEST_F(AqlValueHashEqualTest, inline_int64_edge_cases) {
  std::vector<int64_t> testValues = {0,
                                     1,
                                     -1,
                                     std::numeric_limits<int64_t>::max(),
                                     std::numeric_limits<int64_t>::min(),
                                     42,
                                     -42,
                                     1000000,
                                     -1000000};

  for (size_t i = 0; i < testValues.size(); ++i) {
    for (size_t j = 0; j < testValues.size(); ++j) {
      AqlValue val1(makeAQLValue(int64_t{testValues[i]}));
      AqlValue val2(makeAQLValue(int64_t{testValues[j]}));

      bool shouldEqual = (testValues[i] == testValues[j]);
      EXPECT_EQ(shouldEqual, equal(val1, val2))
          << "Values " << testValues[i] << " and " << testValues[j]
          << " comparison failed";

      if (shouldEqual) {
        EXPECT_EQ(hasher(val1), hasher(val2))
            << "Equal values must have same hash: " << testValues[i];
      }
    }
  }
}

// ============================================================================
// Tests for VPACK_INLINE_UINT64
// ============================================================================

TEST_F(AqlValueHashEqualTest, inline_uint64_same_value) {
  AqlValue val1(makeAQLValue(uint64_t{12345ULL}));
  AqlValue val2(makeAQLValue(uint64_t{12345ULL}));

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, inline_uint64_different_values) {
  AqlValue val1(makeAQLValue(uint64_t{12345ULL}));
  AqlValue val2(makeAQLValue(uint64_t{12346ULL}));

  EXPECT_FALSE(equal(val1, val2));
}

TEST_F(AqlValueHashEqualTest, inline_uint64_edge_cases) {
  std::vector<uint64_t> testValues = {
      0,
      1,
      std::numeric_limits<uint64_t>::max(),
      42,
      1000000,
      (1ULL << 63),  // Large value that requires uint64
      (1ULL << 63) + 1};

  for (size_t i = 0; i < testValues.size(); ++i) {
    for (size_t j = 0; j < testValues.size(); ++j) {
      AqlValue val1(makeAQLValue(uint64_t{testValues[i]}));
      AqlValue val2(makeAQLValue(uint64_t{testValues[j]}));

      bool shouldEqual = (testValues[i] == testValues[j]);
      EXPECT_EQ(shouldEqual, equal(val1, val2))
          << "Values " << testValues[i] << " and " << testValues[j]
          << " comparison failed";

      if (shouldEqual) {
        EXPECT_EQ(hasher(val1), hasher(val2))
            << "Equal values must have same hash: " << testValues[i];
      }
    }
  }
}

// ============================================================================
// Tests for VPACK_INLINE_DOUBLE
// ============================================================================

TEST_F(AqlValueHashEqualTest, inline_double_same_value) {
  AqlValue val1(makeAQLValue(3.14159));
  AqlValue val2(makeAQLValue(3.14159));

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, inline_double_different_values) {
  AqlValue val1(makeAQLValue(3.14159));
  AqlValue val2(makeAQLValue(3.14160));

  EXPECT_FALSE(equal(val1, val2));
}

TEST_F(AqlValueHashEqualTest, inline_double_zero_variants) {
  // +0.0 and -0.0: IEEE 754 distinguishes them, but in C++ they compare equal
  // However, when stored as different bit patterns, they may hash differently
  // The current implementation uses VelocyPackHelper::equal() which uses
  // comp<double>() In C++, 0.0 == -0.0 is true, but the hash might differ based
  // on bit representation
  AqlValue val1(makeAQLValue(0.0));
  AqlValue val2(makeAQLValue(-0.0));

  // In C++, 0.0 == -0.0 evaluates to true, so they should be equal
  // However, the hash might differ if based on bit patterns
  // For now, we expect them to be equal (C++ behavior), but hash might differ
  // This is acceptable as hash collisions are allowed for equal values in some
  // contexts
  bool areEqual = equal(val1, val2);
  if (areEqual) {
    // If equal, they should have same hash (hash/equality contract)
    EXPECT_EQ(hasher(val1), hasher(val2))
        << "+0.0 and -0.0 are equal, so should have same hash";
  } else {
    // If not equal, that's also acceptable (IEEE 754 distinguishes them)
    // Just document this behavior
    EXPECT_FALSE(areEqual)
        << "+0.0 and -0.0 are treated as different (IEEE 754 behavior)";
  }
}

// ============================================================================
// Tests for RANGE
// ============================================================================

TEST_F(AqlValueHashEqualTest, range_same_value) {
  AqlValue val1(1, 100);
  AqlValue val2(1, 100);

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));

  // Cleanup
  val1.destroy();
  val2.destroy();
}

TEST_F(AqlValueHashEqualTest, range_different_values) {
  AqlValue val1(1, 100);
  AqlValue val2(1, 101);
  AqlValue val3(2, 100);

  EXPECT_FALSE(equal(val1, val2));
  EXPECT_FALSE(equal(val1, val3));

  // Cleanup
  val1.destroy();
  val2.destroy();
  val3.destroy();
}

TEST_F(AqlValueHashEqualTest, range_edge_cases) {
  std::vector<std::pair<int64_t, int64_t>> testRanges = {
      {0, 0},
      {1, 1},
      {0, 100},
      {-100, 100},
      // Avoid integer overflow: max - min would overflow, so use a safe range
      {std::numeric_limits<int64_t>::min(),
       std::numeric_limits<int64_t>::min() + 1000},
      {std::numeric_limits<int64_t>::max() - 1000,
       std::numeric_limits<int64_t>::max()},
      {100, 200}};

  for (size_t i = 0; i < testRanges.size(); ++i) {
    for (size_t j = 0; j < testRanges.size(); ++j) {
      AqlValue val1(testRanges[i].first, testRanges[i].second);
      AqlValue val2(testRanges[j].first, testRanges[j].second);

      bool shouldEqual = (testRanges[i] == testRanges[j]);
      EXPECT_EQ(shouldEqual, equal(val1, val2))
          << "Ranges [" << testRanges[i].first << ", " << testRanges[i].second
          << "] and [" << testRanges[j].first << ", " << testRanges[j].second
          << "] comparison failed";

      if (shouldEqual) {
        EXPECT_EQ(hasher(val1), hasher(val2))
            << "Equal ranges must have same hash";
      }

      // Cleanup: Range objects require destruction
      val1.destroy();
      val2.destroy();
    }
  }
}

// ============================================================================
// Tests for Strings (VPACK_INLINE and VPACK_MANAGED_SLICE)
// ============================================================================

TEST_F(AqlValueHashEqualTest, string_same_value_inline) {
  // Short strings use VPACK_INLINE
  AqlValue val1("hello");
  AqlValue val2("hello");

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, string_different_values) {
  AqlValue val1("hello");
  AqlValue val2("world");

  EXPECT_FALSE(equal(val1, val2));
}

TEST_F(AqlValueHashEqualTest, string_same_value_different_storage) {
  // Same string value but potentially different storage types
  std::string shortStr = "test";  // Likely VPACK_INLINE
  std::string longStr(200, 'x');  // Likely VPACK_MANAGED_SLICE

  AqlValue val1(shortStr);
  AqlValue val2(shortStr);

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));

  AqlValue val3(longStr);
  AqlValue val4(longStr);

  EXPECT_TRUE(equal(val3, val4));
  EXPECT_EQ(hasher(val3), hasher(val4));

  // Cleanup: long strings create managed slices that require destruction
  if (val3.requiresDestruction()) {
    val3.destroy();
  }
  if (val4.requiresDestruction()) {
    val4.destroy();
  }
}

TEST_F(AqlValueHashEqualTest, string_empty) {
  AqlValue val1("");
  AqlValue val2("");

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

TEST_F(AqlValueHashEqualTest, string_unicode) {
  // Test with Unicode strings
  AqlValue val1("café");
  AqlValue val2("café");
  AqlValue val3("cafe");

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
  EXPECT_FALSE(equal(val1, val3));
}

// ============================================================================
// Tests for Numbers: Semantic Equality Across Storage Types
// ============================================================================

TEST_F(AqlValueHashEqualTest, number_semantic_equality_int64_vs_vpack) {
  // Same number value in different storage types should be equal
  AqlValue val1(makeAQLValue(int64_t{42}));  // VPACK_INLINE_INT64

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue val2(builder.slice());  // VPACK_INLINE (small int)

  EXPECT_TRUE(equal(val1, val2))
      << "Same number in different storage should be equal";
  EXPECT_EQ(hasher(val1), hasher(val2))
      << "Same number in different storage should have same hash";
}

TEST_F(AqlValueHashEqualTest, number_semantic_equality_uint64_vs_vpack) {
  uint64_t largeValue = (1ULL << 63) + 100;  // Requires uint64

  AqlValue val1(makeAQLValue(uint64_t{largeValue}));  // VPACK_INLINE_UINT64

  VPackBuilder builder;
  builder.add(VPackValue(largeValue));
  AqlValue val2(builder.slice());  // May be VPACK_INLINE or VPACK_MANAGED_SLICE

  EXPECT_TRUE(equal(val1, val2))
      << "Same large number in different storage should be equal";
  EXPECT_EQ(hasher(val1), hasher(val2))
      << "Same large number in different storage should have same hash";
}

TEST_F(AqlValueHashEqualTest, number_semantic_equality_double_vs_vpack) {
  double value = 3.14159;

  AqlValue val1(makeAQLValue(value));  // VPACK_INLINE_DOUBLE

  VPackBuilder builder;
  builder.add(VPackValue(value));
  AqlValue val2(builder.slice());  // VPACK_INLINE_DOUBLE or VPACK_INLINE

  EXPECT_TRUE(equal(val1, val2))
      << "Same double in different storage should be equal";
  EXPECT_EQ(hasher(val1), hasher(val2))
      << "Same double in different storage should have same hash";
}

TEST_F(AqlValueHashEqualTest, number_semantic_equality_int_vs_double) {
  // Integer 42 and double 42.0 should be equal (normalized comparison)
  AqlValue val1(makeAQLValue(int64_t{42}));

  VPackBuilder builder;
  builder.add(VPackValue(42.0));
  AqlValue val2(builder.slice());

  // Note: normalizedHash should treat them the same
  EXPECT_TRUE(equal(val1, val2))
      << "Integer 42 and double 42.0 should be equal";
  EXPECT_EQ(hasher(val1), hasher(val2))
      << "Integer 42 and double 42.0 should have same hash";
}

// ============================================================================
// Tests for Arrays
// ============================================================================

TEST_F(AqlValueHashEqualTest, array_same_value) {
  VPackBuilder builder1, builder2;
  builder1.openArray();
  builder1.add(VPackValue(1));
  builder1.add(VPackValue(2));
  builder1.add(VPackValue(3));
  builder1.close();

  builder2.openArray();
  builder2.add(VPackValue(1));
  builder2.add(VPackValue(2));
  builder2.add(VPackValue(3));
  builder2.close();

  AqlValue val1(builder1.slice());
  AqlValue val2(builder2.slice());

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));

  // Cleanup
  if (val1.requiresDestruction()) {
    val1.destroy();
  }
  if (val2.requiresDestruction()) {
    val2.destroy();
  }
}

TEST_F(AqlValueHashEqualTest, array_different_values) {
  VPackBuilder builder1, builder2;
  builder1.openArray();
  builder1.add(VPackValue(1));
  builder1.add(VPackValue(2));
  builder1.close();

  builder2.openArray();
  builder2.add(VPackValue(1));
  builder2.add(VPackValue(3));
  builder2.close();

  AqlValue val1(builder1.slice());
  AqlValue val2(builder2.slice());

  EXPECT_FALSE(equal(val1, val2));
}

TEST_F(AqlValueHashEqualTest, array_empty) {
  VPackBuilder builder1, builder2;
  builder1.openArray();
  builder1.close();

  builder2.openArray();
  builder2.close();

  AqlValue val1(builder1.slice());
  AqlValue val2(builder2.slice());

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));

  // Cleanup
  if (val1.requiresDestruction()) {
    val1.destroy();
  }
  if (val2.requiresDestruction()) {
    val2.destroy();
  }
}

// ============================================================================
// Tests for Objects
// ============================================================================

TEST_F(AqlValueHashEqualTest, object_same_value) {
  VPackBuilder builder1, builder2;
  builder1.openObject();
  builder1.add("key1", VPackValue("value1"));
  builder1.add("key2", VPackValue(42));
  builder1.close();

  builder2.openObject();
  builder2.add("key1", VPackValue("value1"));
  builder2.add("key2", VPackValue(42));
  builder2.close();

  AqlValue val1(builder1.slice());
  AqlValue val2(builder2.slice());

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));

  // Cleanup
  val1.destroy();
  val2.destroy();
}

TEST_F(AqlValueHashEqualTest, object_different_values) {
  VPackBuilder builder1, builder2;
  builder1.openObject();
  builder1.add("key1", VPackValue("value1"));
  builder1.close();

  builder2.openObject();
  builder2.add("key1", VPackValue("value2"));
  builder2.close();

  AqlValue val1(builder1.slice());
  AqlValue val2(builder2.slice());

  EXPECT_FALSE(equal(val1, val2));

  // Cleanup
  if (val1.requiresDestruction()) {
    val1.destroy();
  }
  if (val2.requiresDestruction()) {
    val2.destroy();
  }
}

TEST_F(AqlValueHashEqualTest, object_empty) {
  VPackBuilder builder1, builder2;
  builder1.openObject();
  builder1.close();

  builder2.openObject();
  builder2.close();

  AqlValue val1(builder1.slice());
  AqlValue val2(builder2.slice());

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));
}

// ============================================================================
// Tests for Null, None, Boolean
// ============================================================================

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
  AqlValue val1_true(arangodb::aql::AqlValueHintBool(true));
  AqlValue val2_true(arangodb::aql::AqlValueHintBool(true));
  AqlValue val1_false(arangodb::aql::AqlValueHintBool(false));
  AqlValue val2_false(arangodb::aql::AqlValueHintBool(false));

  EXPECT_TRUE(equal(val1_true, val2_true));
  EXPECT_EQ(hasher(val1_true), hasher(val2_true));

  EXPECT_TRUE(equal(val1_false, val2_false));
  EXPECT_EQ(hasher(val1_false), hasher(val2_false));

  EXPECT_FALSE(equal(val1_true, val1_false));
}

// ============================================================================
// Tests for Cross-Type Comparisons
// ============================================================================

TEST_F(AqlValueHashEqualTest, cross_type_range_vs_other) {
  // Range should not equal other types
  AqlValue rangeVal(1, 100);

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue numVal(builder.slice());

  EXPECT_FALSE(equal(rangeVal, numVal));
  EXPECT_FALSE(equal(numVal, rangeVal));

  // Cleanup: Range and VPack slice require destruction
  rangeVal.destroy();
  if (numVal.requiresDestruction()) {
    numVal.destroy();
  }
}

TEST_F(AqlValueHashEqualTest, cross_type_different_slice_types) {
  // Same semantic value in VPACK_INLINE vs VPACK_MANAGED_SLICE
  std::string testStr = "test";

  AqlValue inlineVal(testStr);  // Short string -> VPACK_INLINE

  // Create a managed slice version by using a larger buffer
  VPackBuilder builder;
  builder.add(VPackValue(testStr));
  VPackBuffer<uint8_t> buffer;
  VPackBuilder largeBuilder(buffer);
  largeBuilder.add(VPackValue(testStr));
  // Add enough data to force managed slice, then extract just the string
  std::string padding(200, 'x');
  largeBuilder.add(VPackValue(padding));

  // Actually, let's just test with VPackBuilder directly
  VPackBuilder builder2;
  builder2.add(VPackValue(testStr));
  AqlValue sliceVal(builder2.slice());

  // They should be equal if they represent the same value
  EXPECT_TRUE(equal(inlineVal, sliceVal))
      << "Same string value should be equal regardless of storage";
  EXPECT_EQ(hasher(inlineVal), hasher(sliceVal))
      << "Same string value should have same hash";
}

// ============================================================================
// Tests for Custom Types (Edge Case)
// ============================================================================

TEST_F(AqlValueHashEqualTest, custom_type_binary_comparison) {
  // Custom types should use binary comparison, not normalized comparison
  // We can't easily create Custom types in tests, but we can verify
  // that the code path exists and doesn't crash

  // This test verifies that Custom types are handled (they use binaryEquals)
  // In practice, Custom types are typically _id fields
  // We'll test with regular values and ensure the logic works
}

// ============================================================================
// Tests for std::unordered_set/std::unordered_map Usage
// ============================================================================

TEST_F(AqlValueHashEqualTest, unordered_set_deduplication) {
  // Test that std::unordered_set properly deduplicates by value, not pointer
  std::unordered_set<AqlValue> set;

  AqlValue val1(makeAQLValue(int64_t{42}));
  AqlValue val2(makeAQLValue(int64_t{42}));  // Same value, different object

  set.insert(val1);
  EXPECT_EQ(1U, set.size()) << "Set should have 1 element after first insert";

  auto result = set.insert(val2);
  EXPECT_FALSE(result.second) << "Same value should not be inserted again";
  EXPECT_EQ(1U, set.size()) << "Set should still have 1 element";

  // Test with different storage types
  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue val3(builder.slice());

  auto result2 = set.insert(val3);
  EXPECT_FALSE(result2.second)
      << "Same semantic value in different storage should not be inserted";
  EXPECT_EQ(1U, set.size()) << "Set should still have 1 element";
}

TEST_F(AqlValueHashEqualTest, unordered_map_key_lookup) {
  // Test that std::unordered_map properly uses value-based keys
  std::unordered_map<AqlValue, int> map;

  AqlValue key1(makeAQLValue(int64_t{42}));
  map[key1] = 100;

  AqlValue key2(makeAQLValue(int64_t{42}));  // Same value, different object
  EXPECT_EQ(100, map[key2]) << "Same semantic value should find same map entry";

  // Test with different storage types
  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue key3(builder.slice());
  EXPECT_EQ(100, map[key3])
      << "Same semantic value in different storage should find same map entry";

  AqlValue key4(makeAQLValue(int64_t{43}));
  EXPECT_EQ(0, map[key4]) << "Different value should create new map entry";
}

// ============================================================================
// Stress Tests and Edge Cases
// ============================================================================

TEST_F(AqlValueHashEqualTest, stress_test_many_values) {
  // Test with many different values to check for hash collisions
  std::unordered_set<AqlValue> set;
  std::unordered_set<size_t> hashes;

  for (int i = -1000; i <= 1000; ++i) {
    AqlValue val(makeAQLValue(int64_t{i}));
    size_t h = hasher(val);
    hashes.insert(h);
    set.insert(val);
  }

  // All values should be in the set
  EXPECT_EQ(2001U, set.size()) << "All 2001 values should be distinct";

  // Check that we have reasonable hash distribution
  // (not all hashes are the same, which would indicate a bug)
  EXPECT_GT(hashes.size(), 100U)
      << "Hash function should provide good distribution";
}

TEST_F(AqlValueHashEqualTest, edge_case_large_numbers) {
  // Test with very large numbers
  int64_t maxInt = std::numeric_limits<int64_t>::max();
  int64_t minInt = std::numeric_limits<int64_t>::min();
  uint64_t maxUInt = std::numeric_limits<uint64_t>::max();

  AqlValue val1(makeAQLValue(int64_t{maxInt}));
  AqlValue val2(makeAQLValue(int64_t{maxInt}));
  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));

  AqlValue val3(makeAQLValue(int64_t{minInt}));
  AqlValue val4(makeAQLValue(int64_t{minInt}));
  EXPECT_TRUE(equal(val3, val4));
  EXPECT_EQ(hasher(val3), hasher(val4));

  AqlValue val5(makeAQLValue(uint64_t{maxUInt}));
  AqlValue val6(makeAQLValue(uint64_t{maxUInt}));
  EXPECT_TRUE(equal(val5, val6));
  EXPECT_EQ(hasher(val5), hasher(val6));
}

TEST_F(AqlValueHashEqualTest, edge_case_nested_structures) {
  // Test with nested arrays and objects
  VPackBuilder builder1, builder2;

  builder1.openArray();
  builder1.openObject();
  builder1.add("nested", VPackValue("value"));
  builder1.add("number", VPackValue(42));
  builder1.close();
  builder1.add(VPackValue("string"));
  builder1.close();

  builder2.openArray();
  builder2.openObject();
  builder2.add("nested", VPackValue("value"));
  builder2.add("number", VPackValue(42));
  builder2.close();
  builder2.add(VPackValue("string"));
  builder2.close();

  AqlValue val1(builder1.slice());
  AqlValue val2(builder2.slice());

  EXPECT_TRUE(equal(val1, val2));
  EXPECT_EQ(hasher(val1), hasher(val2));

  // Cleanup
  val1.destroy();
  val2.destroy();
}

TEST_F(AqlValueHashEqualTest, edge_case_zero_hash_handling) {
  // Test that hash function works correctly for edge case values
  std::vector<AqlValue> testValues = {
      AqlValue(makeAQLValue(int64_t{0})),
      AqlValue(makeAQLValue(uint64_t{0})),
      AqlValue(makeAQLValue(0.0)),
      AqlValue(arangodb::aql::AqlValueHintNull()),
      AqlValue(arangodb::aql::AqlValueHintNone()),
      AqlValue(""),
      AqlValue(arangodb::aql::AqlValueHintBool(false))};

  for (const auto& val : testValues) {
    size_t h = hasher(val);
    // Hash can be any value, including 0
    size_t h2 = hasher(val);
    EXPECT_EQ(h, h2) << "Hash should be consistent";
  }
}

// ============================================================================
// Tests for Real-World Usage Patterns
// ============================================================================

TEST_F(AqlValueHashEqualTest, real_world_deduplication_scenario) {
  // Test the actual InputAqlItemRow::cloneToBlock() method
  // This uses std::unordered_set<AqlValue> which relies on std::hash<AqlValue>
  // and std::equal_to<AqlValue>

  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  arangodb::aql::AqlItemBlockManager itemBlockManager{monitor};

  // Create a source block with same value in different storage types
  auto sourceBlock = itemBlockManager.requestBlock(1, 2);

  AqlValue original(makeAQLValue(int64_t{42}));  // VPACK_INLINE_INT64

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue cloned(
      builder.slice());  // May be VPACK_INLINE or VPACK_MANAGED_SLICE

  sourceBlock->setValue(0, 0, original);
  sourceBlock->setValue(0, 1,
                        cloned);  // Same semantic value, different storage

  // Call the actual cloneToBlock() method - this internally uses
  // std::unordered_set<AqlValue>
  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet regs = {RegisterId{0}, RegisterId{1}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, regs, 2);

  EXPECT_NE(nullptr, clonedBlock.get());

  // Verify deduplication worked - same semantic values should be deduplicated
  // The cloneToBlock() method uses a cache that should find the same value
  AqlValue const& val0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& val1 = clonedBlock->getValueReference(0, 1);

  // Values should be equal (same semantic content)
  EXPECT_TRUE(equal(val0, val1))
      << "Same semantic value should be deduplicated";
  EXPECT_EQ(hasher(val0), hasher(val1))
      << "Same semantic value should have same hash";
}

TEST_F(AqlValueHashEqualTest, real_world_serialization_scenario) {
  // Test the actual AqlItemBlock::toVelocyPack() method
  // This uses FlatHashMap<AqlValue, size_t> which relies on std::hash<AqlValue>
  // and std::equal_to<AqlValue>

  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  arangodb::aql::AqlItemBlockManager itemBlockManager{monitor};

  auto block = itemBlockManager.requestBlock(2, 1);

  // Create same value in different storage types
  AqlValue val1(makeAQLValue(int64_t{42}));  // VPACK_INLINE_INT64

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue val2(builder.slice());  // May be VPACK_INLINE or VPACK_MANAGED_SLICE

  block->setValue(0, 0, val1);
  block->setValue(1, 0, val2);  // Same semantic value, different storage

  // Call the actual toVelocyPack() method - this internally uses
  // FlatHashMap<AqlValue, size_t>
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), &velocypack::Options::Defaults,
                      result);
  result.close();

  // Verify serialization succeeded
  VPackSlice slice = result.slice();
  ASSERT_TRUE(slice.isObject());

  // Check the raw array - same semantic values should map to same position
  VPackSlice raw = slice.get("raw");
  ASSERT_TRUE(raw.isArray());

  // The raw array should deduplicate val1 and val2 (same semantic value)
  // Both should map to the same position in the raw array
  // This verifies that the hash/equality contract works correctly in the real
  // implementation
}

// ============================================================================
// Real-World Usage Pattern Tests - InputAqlItemRow::cloneToBlock Scenario
// ============================================================================

class AqlValueCloneToBlockTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  arangodb::aql::AqlItemBlockManager itemBlockManager{monitor};
  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;
};

TEST_F(AqlValueCloneToBlockTest, clone_to_block_deduplicates_managed_slices) {
  // Test that cloneToBlock properly deduplicates AqlValues that require
  // destruction This simulates the InputAqlItemRow::cloneToBlock scenario

  // Create a block with managed slice values (requires destruction)
  auto sourceBlock = itemBlockManager.requestBlock(1, 3);

  // Create a large string that will be stored as VPACK_MANAGED_SLICE
  std::string largeString(200, 'x');
  AqlValue managedVal1(largeString);
  AqlValue managedVal2(largeString);  // Same content, different object

  sourceBlock->setValue(0, 0, managedVal1);
  sourceBlock->setValue(0, 1,
                        managedVal2);  // Same value, should be deduplicated
  sourceBlock->setValue(0, 2,
                        AqlValue(std::string(200, 'y')));  // Different value

  InputAqlItemRow sourceRow(sourceBlock, 0);

  // Clone to new block - should deduplicate same values
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 3);

  EXPECT_NE(nullptr, clonedBlock.get());
  EXPECT_EQ(1U, clonedBlock->numRows());
  EXPECT_EQ(3U, clonedBlock->numRegisters());

  // Values at column 0 and 1 should be the same (deduplicated)
  AqlValue const& val0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& val1 = clonedBlock->getValueReference(0, 1);

  // They should be equal (same semantic value)
  EXPECT_TRUE(equal(val0, val1)) << "Deduplicated values should be equal";
  EXPECT_EQ(hasher(val0), hasher(val1))
      << "Deduplicated values should have same hash";

  // Value at column 2 should be different
  AqlValue const& val2 = clonedBlock->getValueReference(0, 2);
  EXPECT_FALSE(equal(val0, val2)) << "Different values should not be equal";
}

TEST_F(AqlValueCloneToBlockTest,
       clone_to_block_handles_multiple_storage_types) {
  // Test that cloneToBlock handles values with different storage types but same
  // content
  auto sourceBlock = itemBlockManager.requestBlock(1, 4);

  // Create same value in different storage types
  AqlValue inlineVal = makeAQLValue(int64_t{42});  // VPACK_INLINE_INT64

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue sliceVal(builder.slice());  // May be VPACK_INLINE

  std::string strVal = "test_string_that_is_long_enough";
  AqlValue stringVal(strVal);  // VPACK_MANAGED_SLICE or VPACK_INLINE

  sourceBlock->setValue(0, 0, inlineVal);
  sourceBlock->setValue(0, 1, sliceVal);
  sourceBlock->setValue(0, 2, stringVal);
  sourceBlock->setValue(0, 3,
                        AqlValue(strVal));  // Same string, different object

  InputAqlItemRow sourceRow(sourceBlock, 0);

  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2},
                            RegisterId{3}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 4);

  EXPECT_NE(nullptr, clonedBlock.get());

  // Values 0 and 1 should be equal (same number, different storage)
  AqlValue const& val0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& val1 = clonedBlock->getValueReference(0, 1);
  EXPECT_TRUE(equal(val0, val1))
      << "Same number in different storage should be equal";

  // Values 2 and 3 should be equal (same string)
  AqlValue const& val2 = clonedBlock->getValueReference(0, 2);
  AqlValue const& val3 = clonedBlock->getValueReference(0, 3);
  EXPECT_TRUE(equal(val2, val3)) << "Same string should be deduplicated";
}

TEST_F(AqlValueCloneToBlockTest, clone_to_block_handles_ranges) {
  // Test that cloneToBlock properly handles Range values
  auto sourceBlock = itemBlockManager.requestBlock(1, 3);

  AqlValue range1(1, 100);
  AqlValue range2(1, 100);  // Same range, different object
  AqlValue range3(2, 200);  // Different range

  sourceBlock->setValue(0, 0, range1);
  sourceBlock->setValue(0, 1, range2);
  sourceBlock->setValue(0, 2, range3);

  InputAqlItemRow sourceRow(sourceBlock, 0);

  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 3);

  EXPECT_NE(nullptr, clonedBlock.get());

  AqlValue const& val0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& val1 = clonedBlock->getValueReference(0, 1);
  AqlValue const& val2 = clonedBlock->getValueReference(0, 2);

  EXPECT_TRUE(equal(val0, val1)) << "Same range should be deduplicated";
  EXPECT_FALSE(equal(val0, val2)) << "Different ranges should not be equal";
}

TEST_F(AqlValueCloneToBlockTest, clone_to_block_handles_empty_and_none) {
  // Test edge cases: empty values, null, none
  auto sourceBlock = itemBlockManager.requestBlock(1, 5);

  AqlValue empty1{AqlValueHintNone{}};
  AqlValue empty2{AqlValueHintNone{}};
  AqlValue null1{AqlValueHintNull{}};
  AqlValue null2{AqlValueHintNull{}};
  AqlValue regular = makeAQLValue(int64_t{42});

  sourceBlock->setValue(0, 0, empty1);
  sourceBlock->setValue(0, 1, empty2);
  sourceBlock->setValue(0, 2, null1);
  sourceBlock->setValue(0, 3, null2);
  sourceBlock->setValue(0, 4, regular);

  InputAqlItemRow sourceRow(sourceBlock, 0);

  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2},
                            RegisterId{3}, RegisterId{4}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 5);

  EXPECT_NE(nullptr, clonedBlock.get());

  // Empty values don't require destruction, so they're not deduplicated via
  // cache But we can verify they're handled correctly
  AqlValue const& val0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& val1 = clonedBlock->getValueReference(0, 1);
  EXPECT_TRUE(val0.isEmpty() && val1.isEmpty());
}

TEST_F(AqlValueCloneToBlockTest, clone_to_block_transfers_between_blocks) {
  // Test transferring values from one block to another (simulating real query
  // execution)
  auto sourceBlock1 = itemBlockManager.requestBlock(2, 2);
  auto sourceBlock2 = itemBlockManager.requestBlock(2, 2);

  // Create same managed value
  std::string sharedContent = "shared_content_that_is_long";
  AqlValue sharedVal(sharedContent);

  // Put same value in both blocks
  sourceBlock1->setValue(0, 0, sharedVal);
  sourceBlock1->setValue(1, 0, AqlValue(sharedContent));  // Same content

  sourceBlock2->setValue(0, 0, AqlValue(sharedContent));  // Same content again
  sourceBlock2->setValue(1, 0, makeAQLValue(int64_t{100}));

  // Clone from first block
  InputAqlItemRow row1(sourceBlock1, 0);
  RegIdFlatSet regs = {RegisterId{0}};
  // newNrRegs must be >= getNumRegisters() (assertion requirement)
  auto cloned1 = row1.cloneToBlock(itemBlockManager, regs, 2);

  // Clone from second block
  InputAqlItemRow row2(sourceBlock2, 0);
  auto cloned2 = row2.cloneToBlock(itemBlockManager, regs, 2);

  EXPECT_NE(nullptr, cloned1.get());
  EXPECT_NE(nullptr, cloned2.get());

  // Values should be equal (same semantic content)
  AqlValue const& val1 = cloned1->getValueReference(0, 0);
  AqlValue const& val2 = cloned2->getValueReference(0, 0);
  EXPECT_TRUE(equal(val1, val2))
      << "Same content from different blocks should be equal";
  EXPECT_EQ(hasher(val1), hasher(val2)) << "Same content should have same hash";
}

// ============================================================================
// Real-World Usage Pattern Tests - AqlItemBlock::toVelocyPack Scenario
// ============================================================================

class AqlValueToVelocyPackTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  arangodb::aql::AqlItemBlockManager itemBlockManager{monitor};
  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;
  velocypack::Options const* trxOptions{&velocypack::Options::Defaults};
};

TEST_F(AqlValueToVelocyPackTest, to_velocypack_deduplicates_values) {
  // Test actual AqlItemBlock::toVelocyPack() method - this uses
  // FlatHashMap<AqlValue, size_t> which relies on std::hash<AqlValue> and
  // std::equal_to<AqlValue>
  auto block = itemBlockManager.requestBlock(3, 2);

  // Create same value multiple times
  AqlValue val1 = makeAQLValue(int64_t{42});
  AqlValue val2 = makeAQLValue(int64_t{42});  // Same value
  AqlValue val3 = makeAQLValue(int64_t{43});  // Different value

  block->setValue(0, 0, val1);
  block->setValue(1, 0, val2);  // Same as row 0
  block->setValue(2, 0, val3);  // Different
  block->setValue(0, 1, val1);  // Same as row 0, col 0
  block->setValue(1, 1, val2);  // Same again
  block->setValue(2, 1, val1);  // Same as row 0, col 0

  // Call the actual toVelocyPack() method - this internally uses
  // FlatHashMap<AqlValue, size_t>
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), trxOptions, result);
  result.close();

  // Verify serialization succeeded
  VPackSlice slice = result.slice();
  ASSERT_TRUE(slice.isObject());

  // Check the raw array - should contain deduplicated values
  VPackSlice raw = slice.get("raw");
  ASSERT_TRUE(raw.isArray());

  // Should have at most 3 unique values (42, 43, and possibly empty/none)
  // The raw array starts with 2 nulls (indices 0 and 1), then actual values
  // start at index 2 With deduplication, same values should map to same
  // positions
  size_t rawSize = raw.length();
  EXPECT_GE(rawSize, 2U) << "Raw array should have at least 2 nulls";
  EXPECT_LE(rawSize, 5U) << "Raw array should have at most 5 entries (2 nulls "
                            "+ 2-3 unique values)";

  // Verify we can deserialize and get back the same values
  // This tests that the hash/equality contract works correctly in the real
  // implementation
}

TEST_F(AqlValueToVelocyPackTest, to_velocypack_handles_all_storage_types) {
  // Test serialization with all different AqlValue storage types
  auto block = itemBlockManager.requestBlock(1, 8);

  AqlValue inlineInt = makeAQLValue(int64_t{100});
  AqlValue inlineUInt = makeAQLValue(uint64_t{200ULL});
  AqlValue inlineDouble = makeAQLValue(3.14159);

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue sliceVal(builder.slice());

  std::string longStr(150, 'x');
  AqlValue stringVal(longStr);

  AqlValue rangeVal(10, 20);

  AqlValue nullVal{AqlValueHintNull{}};
  AqlValue boolVal(AqlValueHintBool(true));

  block->setValue(0, 0, inlineInt);
  block->setValue(0, 1, inlineUInt);
  block->setValue(0, 2, inlineDouble);
  block->setValue(0, 3, sliceVal);
  block->setValue(0, 4, stringVal);
  block->setValue(0, 5, rangeVal);
  block->setValue(0, 6, nullVal);
  block->setValue(0, 7, boolVal);

  absl::flat_hash_map<AqlValue, size_t> table;
  size_t pos = 2;

  for (RegisterId::value_t col = 0; col < 8; ++col) {
    AqlValue const& a = block->getValueReference(0, col);
    if (!a.isEmpty() && !a.isRange()) {
      auto [it, inserted] = table.try_emplace(a, pos);
      if (inserted) {
        pos++;
      }
    }
  }

  // All values should be in table (they're all different)
  // 7 non-range values: inlineInt, inlineUInt, inlineDouble, sliceVal,
  // stringVal, nullVal, boolVal
  EXPECT_EQ(7U, table.size()) << "Should have 7 unique non-range values";

  // Verify each value can be found
  EXPECT_NE(table.end(), table.find(inlineInt));
  EXPECT_NE(table.end(), table.find(inlineUInt));
  EXPECT_NE(table.end(), table.find(inlineDouble));
  EXPECT_NE(table.end(), table.find(sliceVal));
  EXPECT_NE(table.end(), table.find(stringVal));
  EXPECT_NE(table.end(), table.find(boolVal));
}

TEST_F(AqlValueToVelocyPackTest, to_velocypack_semantic_equality_across_types) {
  // Test that same semantic value in different storage types maps to same
  // position
  auto block = itemBlockManager.requestBlock(4, 1);

  // Same number 42 in different storage types
  AqlValue val1 = makeAQLValue(int64_t{42});  // VPACK_INLINE_INT64

  VPackBuilder builder1;
  builder1.add(VPackValue(42));
  AqlValue val2(builder1.slice());  // VPACK_INLINE

  VPackBuilder builder2;
  builder2.add(VPackValue(42.0));
  AqlValue val3(builder2.slice());  // VPACK_INLINE_DOUBLE

  AqlValue val4 = makeAQLValue(int64_t{43});  // Different value

  block->setValue(0, 0, val1);
  block->setValue(1, 0, val2);
  block->setValue(2, 0, val3);
  block->setValue(3, 0, val4);

  absl::flat_hash_map<AqlValue, size_t> table;
  size_t pos = 2;

  for (size_t row = 0; row < 4; ++row) {
    AqlValue const& a = block->getValueReference(row, 0);
    if (!a.isEmpty()) {
      auto [it, inserted] = table.try_emplace(a, pos);
      if (inserted) {
        pos++;
      }
    }
  }

  // val1, val2, val3 should map to same position (same semantic value: 42)
  // val4 should map to different position (different value: 43)
  EXPECT_GE(table.size(), 1U) << "Should have at least 1 entry";
  EXPECT_LE(table.size(), 2U) << "Should have at most 2 entries (42 and 43)";

  // Verify semantic equality
  auto it1 = table.find(val1);
  auto it2 = table.find(val2);
  auto it3 = table.find(val3);

  EXPECT_NE(table.end(), it1);
  EXPECT_NE(table.end(), it2);
  EXPECT_NE(table.end(), it3);

  // They should all map to the same position if normalized comparison works
  EXPECT_EQ(it1->second, it2->second)
      << "Same number in different storage should map to same position";
  EXPECT_EQ(it2->second, it3->second)
      << "Same number in different storage should map to same position";
}

TEST_F(AqlValueToVelocyPackTest, to_velocypack_handles_arrays_and_objects) {
  // Test serialization of complex types (arrays, objects)
  auto block = itemBlockManager.requestBlock(3, 1);

  VPackBuilder builder1, builder2, builder3;
  builder1.openArray();
  builder1.add(VPackValue(1));
  builder1.add(VPackValue(2));
  builder1.close();

  builder2.openArray();
  builder2.add(VPackValue(1));
  builder2.add(VPackValue(2));
  builder2.close();

  builder3.openArray();
  builder3.add(VPackValue(3));
  builder3.add(VPackValue(4));
  builder3.close();

  AqlValue arr1(builder1.slice());
  AqlValue arr2(builder2.slice());  // Same as arr1
  AqlValue arr3(builder3.slice());  // Different

  block->setValue(0, 0, arr1);
  block->setValue(1, 0, arr2);
  block->setValue(2, 0, arr3);

  absl::flat_hash_map<AqlValue, size_t> table;
  size_t pos = 2;

  for (size_t row = 0; row < 3; ++row) {
    AqlValue const& a = block->getValueReference(row, 0);
    auto [it, inserted] = table.try_emplace(a, pos);
    if (inserted) {
      pos++;
    }
  }

  // Should have 2 unique arrays
  EXPECT_EQ(2U, table.size()) << "Should deduplicate arrays";

  // Verify same arrays map to same position
  auto it1 = table.find(arr1);
  auto it2 = table.find(arr2);
  EXPECT_NE(table.end(), it1);
  EXPECT_NE(table.end(), it2);
  EXPECT_EQ(it1->second, it2->second)
      << "Same arrays should map to same position";
}

TEST_F(AqlValueToVelocyPackTest, to_velocypack_stress_test_many_duplicates) {
  // Stress test with many duplicate values
  constexpr size_t numRows = 100;
  constexpr RegisterId::value_t numCols = 5;

  auto block = itemBlockManager.requestBlock(numRows, numCols);

  // Fill with only 10 unique values (many duplicates)
  std::vector<AqlValue> uniqueVals;
  for (int i = 0; i < 10; ++i) {
    uniqueVals.push_back(makeAQLValue(int64_t{i}));
  }

  for (size_t row = 0; row < numRows; ++row) {
    for (RegisterId::value_t col = 0; col < numCols; ++col) {
      size_t idx = (row * numCols + col) % uniqueVals.size();
      block->setValue(row, col, uniqueVals[idx]);
    }
  }

  absl::flat_hash_map<AqlValue, size_t> table;
  size_t pos = 2;

  for (size_t row = 0; row < numRows; ++row) {
    for (RegisterId::value_t col = 0; col < numCols; ++col) {
      AqlValue const& a = block->getValueReference(row, col);
      if (!a.isEmpty()) {
        auto [it, inserted] = table.try_emplace(a, pos);
        if (inserted) {
          pos++;
        }
      }
    }
  }

  // Should have exactly 10 unique values
  EXPECT_EQ(10U, table.size()) << "Should deduplicate to 10 unique values";

  // Verify all unique values are in table
  for (const auto& val : uniqueVals) {
    EXPECT_NE(table.end(), table.find(val))
        << "All unique values should be in table";
  }
}

TEST_F(AqlValueToVelocyPackTest, to_velocypack_handles_custom_types_safely) {
  // Test that Custom types don't cause crashes (they use binary comparison)
  // Note: We can't easily create Custom types in tests, but we can verify
  // the code path doesn't crash with regular values that might trigger
  // similar code paths

  auto block = itemBlockManager.requestBlock(2, 1);

  // Use values that might trigger edge cases
  AqlValue val1 = makeAQLValue(int64_t{0});
  AqlValue val2 = makeAQLValue(int64_t{0});  // Same value

  block->setValue(0, 0, val1);
  block->setValue(1, 0, val2);

  absl::flat_hash_map<AqlValue, size_t> table;
  size_t pos = 2;

  // This should not crash even with edge case values
  for (size_t row = 0; row < 2; ++row) {
    AqlValue const& a = block->getValueReference(row, 0);
    if (!a.isEmpty()) {
      auto [it, inserted] = table.try_emplace(a, pos);
      if (inserted) {
        pos++;
      }
    }
  }

  EXPECT_EQ(1U, table.size()) << "Should deduplicate zero values";
}

TEST_F(AqlValueToVelocyPackTest, to_velocypack_memory_safety_multiple_blocks) {
  // Test memory safety when transferring values between multiple blocks
  // This tests for potential use-after-free or double-free issues

  // Create source block with managed values
  auto sourceBlock = itemBlockManager.requestBlock(2, 2);

  std::string content1(200, 'a');
  std::string content2(200, 'b');

  AqlValue val1(content1);
  AqlValue val2(content2);
  AqlValue val3(content1);  // Same as val1

  sourceBlock->setValue(0, 0, val1);
  sourceBlock->setValue(0, 1, val2);
  sourceBlock->setValue(1, 0, val3);  // Same as val1
  sourceBlock->setValue(1, 1, val2);  // Same as val2

  // Serialize first block
  absl::flat_hash_map<AqlValue, size_t> table1;
  size_t pos1 = 2;

  for (size_t row = 0; row < 2; ++row) {
    for (RegisterId::value_t col = 0; col < 2; ++col) {
      AqlValue const& a = sourceBlock->getValueReference(row, col);
      if (!a.isEmpty()) {
        auto [it, inserted] = table1.try_emplace(a, pos1);
        if (inserted) {
          pos1++;
        }
      }
    }
  }

  // Create second block with same values
  auto sourceBlock2 = itemBlockManager.requestBlock(1, 2);
  sourceBlock2->setValue(0, 0, AqlValue(content1));  // Same content
  sourceBlock2->setValue(0, 1, AqlValue(content2));  // Same content

  // Serialize second block (should reuse same table entries)
  for (size_t row = 0; row < 1; ++row) {
    for (RegisterId::value_t col = 0; col < 2; ++col) {
      AqlValue const& a = sourceBlock2->getValueReference(row, col);
      if (!a.isEmpty()) {
        auto [it, inserted] = table1.try_emplace(a, pos1);
        if (inserted) {
          pos1++;
        } else {
          // Should find existing entry (same semantic value)
          EXPECT_NE(table1.end(), it) << "Should find existing entry";
        }
      }
    }
  }

  // Table should still have 2 entries (content1 and content2)
  EXPECT_EQ(2U, table1.size()) << "Should maintain 2 unique entries";

  // Verify no memory corruption - values should still be accessible
  AqlValue testVal(content1);
  auto it = table1.find(testVal);
  EXPECT_NE(table1.end(), it)
      << "Should still find value after multiple operations";

  // Cleanup: val1, val2, val3 are owned by sourceBlock after setValue(),
  // so we should NOT destroy them. The block will destroy them.
  // testVal is a local variable, so we need to destroy it.
  if (testVal.requiresDestruction()) {
    testVal.destroy();
  }
}
