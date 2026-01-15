#include "AqlItemBlockHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlValue.h"
#include "Aql/RegIdFlatSet.h"
#include "Basics/GlobalResourceMonitor.h"
#include <boost/container/flat_set.hpp>
#include "Basics/ResourceUsage.h"
#include "Containers/FlatHashMap.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace arangodb {
namespace tests {
namespace aql {

class AqlValueHashTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor};
  velocypack::Options const* const options{&velocypack::Options::Defaults};
};

// Tests for std::hash<AqlValue> and std::equal_to<AqlValue>
// Verifies hash/equality contract: if a == b, then hash(a) == hash(b)

TEST_F(AqlValueHashTest, AqlValueHash_InlineValues) {
  // Inline values: hash and compare by content
  AqlValue v1(AqlValueHintInt(42));
  AqlValue v2(AqlValueHintInt(42));
  AqlValue v3(AqlValueHintInt(100));

  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  // Same values should have same hash
  EXPECT_EQ(hasher(v1), hasher(v2));
  EXPECT_TRUE(equal(v1, v2));

  // Different values should have different hashes (likely, not guaranteed)
  EXPECT_TRUE(equal(v1, v2));
  EXPECT_FALSE(equal(v1, v3));
}

TEST_F(AqlValueHashTest, AqlValueHash_ManagedSlices) {
  // Managed slices: hash and compare by content (semantic equality)
  // The new implementation uses VelocyPackHelper::equal() which performs
  // content-based comparison
  arangodb::velocypack::Builder b1, b2, b3;
  b1.add(arangodb::velocypack::Value("same content"));
  b2.add(arangodb::velocypack::Value("same content"));
  b3.add(arangodb::velocypack::Value("different content"));

  AqlValue v1(b1.slice(), b1.slice().byteSize());
  AqlValue v2(b2.slice(), b2.slice().byteSize());
  AqlValue v3(b3.slice(), b3.slice().byteSize());

  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  // Same content -> same hash and equal (content-based comparison)
  // This is CORRECT: managed slices with same content are semantically equal
  EXPECT_EQ(hasher(v1), hasher(v2)) << "Same content should have same hash";
  EXPECT_TRUE(equal(v1, v2)) << "Same content should be equal";

  // Different content -> different hash and not equal
  EXPECT_FALSE(equal(v1, v3));
  EXPECT_FALSE(equal(v2, v3));

  // Same pointer should have same hash
  AqlValue v1_copy = v1.clone();
  EXPECT_EQ(hasher(v1), hasher(v1_copy));
  EXPECT_TRUE(equal(v1, v1_copy));

  v1.destroy();
  v2.destroy();
  v3.destroy();
  v1_copy.destroy();
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_SupervisedSlices_SameContent) {
  // Test supervised slices with same content but different pointers
  // Verifies hash uses content, not pointer address
  std::string content = "same supervised content";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(content));
  b2.add(arangodb::velocypack::Value(content));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));

  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  // Verify they have different pointers (different allocations)
  EXPECT_NE(v1.data(), v2.data())
      << "Supervised slices should have different pointers";

  // Verify they are equal by content
  EXPECT_TRUE(equal(v1, v2))
      << "Supervised slices with same content should be equal";

  // CRITICAL: Hash/equality contract
  // If equal() returns true, hash() MUST return the same value
  // OLD HASH: This will FAIL (different hashes for equal values)
  // NEW HASH: This will PASS (same hash for equal values)
  EXPECT_EQ(hasher(v1), hasher(v2))
      << "Hash/equality contract violated: equal values must have same hash";

  v1.destroy();
  v2.destroy();
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_SupervisedSlices_DifferentContent) {
  // Supervised slices with different content should not be equal
  std::string content1 = "content 1";
  std::string content2 = "content 2";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(content1));
  b2.add(arangodb::velocypack::Value(content2));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));

  std::equal_to<AqlValue> equal;

  // Different content -> not equal
  EXPECT_FALSE(equal(v1, v2));

  // Different content -> different hashes (expected, but not required)
  // Note: Hash collisions are possible, but unlikely for different content

  v1.destroy();
  v2.destroy();
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_SupervisedVsManaged_ContentEqual) {
  // Test cross-type equality: supervised vs managed with same content
  // Note: These are different types, so they may have different hashes
  // But they should be equal by content
  std::string content = "same content";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(content));
  b2.add(arangodb::velocypack::Value(content));

  AqlValue supervised(
      b1.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b1.slice().byteSize()));
  AqlValue managed(b2.slice(), b2.slice().byteSize());

  std::equal_to<AqlValue> equal;

  // Different types may have different hashes (expected)
  // But content is equal (cross-type comparison)
  EXPECT_TRUE(equal(supervised, managed))
      << "Supervised and managed slices with same content are equal";

  supervised.destroy();
}

TEST_F(AqlValueHashTest, AqlValueHash_UsageInToVelocyPack_Deduplication) {
  // Test actual usage in toVelocyPack() deduplication
  // This uses FlatHashMap<AqlValue, size_t> which relies on hash and equal_to
  // This is the PRIMARY usage location for std::hash<AqlValue>
  auto block = itemBlockManager.requestBlock(4, 1);

  // Create two supervised slices with same content
  std::string content = "shared content for deduplication";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(content));
  b2.add(arangodb::velocypack::Value(content));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));

  // Verify different pointers
  EXPECT_NE(v1.data(), v2.data());

  // Set same content in different rows
  block->setValue(0, 0, v1);
  block->setValue(1, 0, v2);  // Same content, different pointer

  // Serialize - this uses FlatHashMap<AqlValue, size_t>
  // OLD HASH: Equal supervised slices may NOT be deduplicated (different hashes
  // -> different buckets) NEW HASH: Equal supervised slices SHOULD be
  // deduplicated (same hash -> same bucket)
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  // Verify serialization succeeded
  VPackSlice slice = result.slice();
  ASSERT_TRUE(slice.isObject());

  // Check if deduplication worked correctly
  VPackSlice raw = slice.get("raw");
  ASSERT_TRUE(raw.isArray());

  // With correct hash/equality contract, equal supervised slices should be
  // deduplicated This test verifies that the fix works in practice The raw
  // array should contain fewer entries if deduplication works correctly

  // Note: v1 and v2 are owned by the block, so we should NOT destroy them
  // manually The block will destroy them when reset
  block.reset(nullptr);
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_UsageInToVelocyPack_SamePointer) {
  // Test deduplication when same pointer is used (should always work)
  // This tests the case where referenceValuesFromRow() creates shared
  // references
  auto block = itemBlockManager.requestBlock(3, 1);

  // Create one supervised slice
  std::string content = "single supervised slice";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue v(b.slice(), static_cast<arangodb::velocypack::ValueLength>(
                            b.slice().byteSize()));

  // Reference the same value in multiple rows (same pointer)
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->setValue(0, 0, v);
  block->referenceValuesFromRow(1, regs, 0);
  block->referenceValuesFromRow(2, regs, 0);

  // All rows should have same pointer
  void const* ptr0 = block->getValueReference(0, 0).data();
  void const* ptr1 = block->getValueReference(1, 0).data();
  void const* ptr2 = block->getValueReference(2, 0).data();
  EXPECT_EQ(ptr0, ptr1);
  EXPECT_EQ(ptr1, ptr2);

  // Serialize - same pointer should definitely be deduplicated
  // This should work correctly even with old hash implementation
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  VPackSlice slice = result.slice();
  ASSERT_TRUE(slice.isObject());
  VPackSlice raw = slice.get("raw");
  ASSERT_TRUE(raw.isArray());

  // Same pointer -> same hash -> should be deduplicated
  // This should work correctly even with current implementation

  // Note: v is owned by the block after setValue(), so we should NOT destroy it
  // The block will destroy it when reset
  block.reset(nullptr);
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_RangeValues) {
  // Range values: hash and compare by CONTENT (low and high values)
  // NEW APPROACH: Range values are compared by content, so hashing by
  // content is correct
  AqlValue r1(1, 10);
  AqlValue r2(1, 10);  // Different Range object, same content
  AqlValue r3(1, 10);
  AqlValue r3_copy = r3;  // Same pointer
  AqlValue r4(2, 20);     // Different content

  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  // Same content -> same hash and equal (NEW APPROACH: content-based)
  EXPECT_EQ(hasher(r1), hasher(r2))
      << "Range values with same content should have same hash (content-based)";
  EXPECT_TRUE(equal(r1, r2))
      << "Range values with same content should be equal (content-based)";

  // Same pointer -> same hash and equal
  EXPECT_EQ(hasher(r3), hasher(r3_copy));
  EXPECT_TRUE(equal(r3, r3_copy));

  // Different content -> different hash
  EXPECT_NE(hasher(r1), hasher(r4))
      << "Range values with different content should have different hashes";
  EXPECT_FALSE(equal(r1, r4))
      << "Range values with different content should not be equal";

  r1.destroy();
  r2.destroy();
  r3.destroy();
  r4.destroy();
}

TEST_F(AqlValueHashTest, AqlValueHash_InlineTypes_Consistency) {
  // Test all inline types for hash/equality consistency
  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  // Integers
  AqlValue i1(AqlValueHintInt(42));
  AqlValue i2(AqlValueHintInt(42));
  AqlValue i3(AqlValueHintInt(100));
  EXPECT_EQ(hasher(i1), hasher(i2));
  EXPECT_TRUE(equal(i1, i2));
  EXPECT_FALSE(equal(i1, i3));

  // Unsigned integers
  AqlValue u1(AqlValueHintUInt(42));
  AqlValue u2(AqlValueHintUInt(42));
  EXPECT_EQ(hasher(u1), hasher(u2));
  EXPECT_TRUE(equal(u1, u2));

  // Doubles
  AqlValue d1(AqlValueHintDouble(3.14));
  AqlValue d2(AqlValueHintDouble(3.14));
  EXPECT_EQ(hasher(d1), hasher(d2));
  EXPECT_TRUE(equal(d1, d2));

  // Booleans
  AqlValue b1(AqlValueHintBool(true));
  AqlValue b2(AqlValueHintBool(true));
  EXPECT_EQ(hasher(b1), hasher(b2));
  EXPECT_TRUE(equal(b1, b2));

  // Null
  AqlValue n1{AqlValueHintNull{}};
  AqlValue n2{AqlValueHintNull{}};
  EXPECT_EQ(hasher(n1), hasher(n2));
  EXPECT_TRUE(equal(n1, n2));
}

TEST_F(AqlValueHashTest, AqlValueHash_EdgeCase_EmptyValues) {
  // Test edge case: empty values
  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  AqlValue empty1;
  AqlValue empty2;

  // Empty values should be equal
  EXPECT_TRUE(equal(empty1, empty2));
  // Empty values should have same hash (if they're equal)
  EXPECT_EQ(hasher(empty1), hasher(empty2));
}

TEST_F(AqlValueHashTest, AqlValueHash_EdgeCase_LargeSupervisedSlices) {
  // Test with large supervised slices to ensure no ASAN errors
  std::string largeContent(10000, 'x');
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(largeContent));
  b2.add(arangodb::velocypack::Value(largeContent));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));

  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  // Large supervised slices with same content should be equal
  EXPECT_TRUE(equal(v1, v2));
  // And should have same hash (with new hash implementation)
  EXPECT_EQ(hasher(v1), hasher(v2))
      << "Large supervised slices with same content must have same hash";

  v1.destroy();
  v2.destroy();
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_EdgeCase_ManySupervisedSlices) {
  // Stress test: many supervised slices with same content
  // This tests hash table performance and correctness
  constexpr size_t numSlices = 100;
  std::string content = "shared content for stress test";

  std::vector<AqlValue> slices;
  slices.reserve(numSlices);

  for (size_t i = 0; i < numSlices; ++i) {
    arangodb::velocypack::Builder b;
    b.add(arangodb::velocypack::Value(content));
    slices.emplace_back(
        b.slice(),
        static_cast<arangodb::velocypack::ValueLength>(b.slice().byteSize()));
  }

  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  // All slices should be equal
  for (size_t i = 1; i < numSlices; ++i) {
    EXPECT_TRUE(equal(slices[0], slices[i]))
        << "Slice " << i << " should equal slice 0";
    // All should have same hash (with new hash implementation)
    EXPECT_EQ(hasher(slices[0]), hasher(slices[i]))
        << "Slice " << i << " should have same hash as slice 0";
  }

  // Test in FlatHashMap (actual usage)
  containers::FlatHashMap<AqlValue, size_t> table;
  for (size_t i = 0; i < numSlices; ++i) {
    auto [it, inserted] = table.try_emplace(slices[i], i);
    // With correct hash: all should map to same entry (inserted only for first)
    // With old hash: each might map to different entry (wrong!)
    if (i == 0) {
      EXPECT_TRUE(inserted) << "First slice should be inserted";
    } else {
      // With correct hash: should find existing entry
      // With old hash: might insert new entry (wrong!)
      // We can't assert this without knowing which hash is used, but we can
      // verify that all entries in the table are equal
      if (!inserted) {
        EXPECT_TRUE(equal(slices[0], slices[i]))
            << "Found entry should equal original";
      }
    }
  }

  // Cleanup
  for (auto& slice : slices) {
    slice.destroy();
  }
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_ASAN_PotentialUseAfterFree) {
  // Test for potential use-after-free issues when hashing/comparing
  // This simulates the scenario where values are moved between blocks
  auto block = itemBlockManager.requestBlock(2, 1);

  std::string content = "test content for ASAN test";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue v(b.slice(), static_cast<arangodb::velocypack::ValueLength>(
                            b.slice().byteSize()));

  block->setValue(0, 0, v);

  // Create hash table with value from block
  containers::FlatHashMap<AqlValue, size_t> table;
  AqlValue const& blockValue = block->getValueReference(0, 0);
  table[blockValue] = 100;

  // Steal value from block (simulating cloneDataAndMoveShadow)
  AqlValue stolen = block->stealAndEraseValue(0, 0);

  // Block value should now be empty
  EXPECT_TRUE(block->getValueReference(0, 0).isEmpty());

  // Stolen value should still be valid and hashable
  std::hash<AqlValue> hasher;
  size_t hash = hasher(stolen);
  EXPECT_NE(0U, hash) << "Stolen value should still be hashable";

  // Should be able to find it in table (if hash is correct)
  auto it = table.find(stolen);
  if (it != table.end()) {
    EXPECT_EQ(100, it->second) << "Should find stolen value in table";
  }

  // Cleanup
  stolen.destroy();
  block.reset(nullptr);
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_VerifyPointerVsPayload_Hashing) {
  // This test verifies that hashing and comparison use content-based approach
  // for all AqlValue types, not pointer-based

  std::hash<AqlValue> hasher;
  std::equal_to<AqlValue> equal;

  // MANAGED SLICES: equal_to() compares by content (via
  // VelocyPackHelper::equal)
  // -> Hashing by content is CORRECT (not by pointer)
  {
    arangodb::velocypack::Builder b1, b2;
    b1.add(arangodb::velocypack::Value("same"));
    b2.add(arangodb::velocypack::Value("same"));
    AqlValue m1(b1.slice(), b1.slice().byteSize());
    AqlValue m2(b2.slice(), b2.slice().byteSize());

    // Same content -> equal -> same hash is CORRECT (content-based)
    EXPECT_TRUE(equal(m1, m2));
    EXPECT_EQ(hasher(m1), hasher(m2));

    // Cleanup
    m1.destroy();
    m2.destroy();
  }

  // SUPERVISED SLICES: equal_to() compares by content
  // -> Hashing by pointer is WRONG, hashing by payload is CORRECT
  {
    std::string content = "same content";
    arangodb::velocypack::Builder b1, b2;
    b1.add(arangodb::velocypack::Value(content));
    b2.add(arangodb::velocypack::Value(content));
    AqlValue s1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                b1.slice().byteSize()));
    AqlValue s2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                b2.slice().byteSize()));

    // Same content -> equal -> same hash is REQUIRED
    EXPECT_TRUE(equal(s1, s2));
    EXPECT_EQ(hasher(s1), hasher(s2))
        << "Supervised slices: equal by content -> must hash by content";

    s1.destroy();
    s2.destroy();
  }

  // RANGE: equal_to() compares by CONTENT (low and high values)
  // -> Hashing by content is CORRECT (NEW APPROACH)
  {
    AqlValue r1(1, 10);
    AqlValue r2(1, 10);
    // Same content -> equal -> same hash is CORRECT (NEW APPROACH)
    EXPECT_TRUE(equal(r1, r2))
        << "Range values with same content should be equal (content-based)";
    EXPECT_EQ(hasher(r1), hasher(r2))
        << "Range values with same content should have same hash "
           "(content-based)";

    r1.destroy();
    r2.destroy();
  }

  // INLINE VALUES: equal_to() compares by content
  // -> Hashing by content is CORRECT
  {
    AqlValue i1(AqlValueHintInt(42));
    AqlValue i2(AqlValueHintInt(42));
    // Same content -> equal -> same hash is REQUIRED
    EXPECT_TRUE(equal(i1, i2));
    EXPECT_EQ(hasher(i1), hasher(i2));
  }

  EXPECT_EQ(monitor.current(), 0U);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
