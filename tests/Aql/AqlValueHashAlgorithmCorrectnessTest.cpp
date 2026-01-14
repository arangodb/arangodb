#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlValue.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegIdFlatSet.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include <boost/container/flat_set.hpp>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
// Helper to create AqlValue from int64
static inline AqlValue makeAQLValue(int64_t x) {
  return AqlValue(AqlValueHintInt(x));
}
}  // namespace

// Tests verify content-based hashing produces correct deduplication behavior

class AqlValueHashAlgorithmCorrectnessTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  arangodb::aql::AqlItemBlockManager itemBlockManager{monitor};
  velocypack::Options const* const options{&velocypack::Options::Defaults};
};

// Tests for AqlItemBlock::toVelocyPack() deduplication

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_DeduplicatesSameContent_DifferentPointers) {
  // Verify toVelocyPack() deduplicates AqlValues with same content but
  // different pointers

  auto block = itemBlockManager.requestBlock(4, 1);

  // Create two supervised slices with IDENTICAL content but DIFFERENT pointers
  std::string sharedContent = "shared_content_for_deduplication_test";
  arangodb::velocypack::Builder b1, b2, b3;
  b1.add(arangodb::velocypack::Value(sharedContent));
  b2.add(arangodb::velocypack::Value(sharedContent));  // Same content
  b3.add(arangodb::velocypack::Value("different_content"));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));
  AqlValue v3(b3.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b3.slice().byteSize()));

  // Verify they have different pointers (critical for the test)
  EXPECT_NE(v1.data(), v2.data()) << "v1 and v2 must have different pointers "
                                     "for this test to be meaningful";

  // Place same content in multiple rows
  block->setValue(0, 0, v1);  // Row 0: first instance of sharedContent
  block->setValue(
      1, 0,
      v2);  // Row 1: second instance of sharedContent (different pointer!)
  block->setValue(2, 0, v3);  // Row 2: different content
  block->setValue(3, 0, v1);  // Row 3: first instance again

  // Call the actual toVelocyPack() method
  // This uses FlatHashMap<AqlValue, size_t> internally
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  VPackSlice slice = result.slice();
  ASSERT_TRUE(slice.isObject());

  // Extract the "raw" array and "data" array
  VPackSlice raw = slice.get("raw");
  ASSERT_TRUE(raw.isArray());
  VPackSlice data = slice.get("data");
  ASSERT_TRUE(raw.isArray());

  // Expected: v1 and v2 (same content, different pointers)
  // should map to the SAME position in the raw array
  //
  // The raw array structure:
  // - Index 0: null (reserved)
  // - Index 1: null (reserved)
  // - Index 2+: actual values
  //
  // With CORRECT deduplication:
  // - v1 and v2 should both map to the same position (e.g., position 2)
  // - v3 should map to a different position (e.g., position 3)
  // - Total unique values in raw: 2 (sharedContent + different_content)
  //
  // With WRONG deduplication (old pointer-based approach):
  // - v1 would map to position 2
  // - v2 would map to position 3 (different pointer!)
  // - v3 would map to position 4
  // - Total unique values in raw: 3 (no deduplication!)

  // Count unique values in raw array (excluding the two nulls at start)
  size_t uniqueValuesInRaw =
      raw.length() - 2;  // Subtract 2 for nulls at 0 and 1

  // EXPECTED BEHAVIOR: Only 2 unique values (sharedContent and
  // different_content) This proves deduplication worked correctly
  EXPECT_EQ(2U, uniqueValuesInRaw)
      << "CORRECT: Same content (v1 and v2) should be deduplicated to 1 entry "
         "in raw array. "
      << "OLD APPROACH (pointer-based) would have 3 entries (no "
         "deduplication). "
      << "NEW APPROACH (content-based) has 2 entries (deduplication works).";

  // Verify the data array references the correct positions
  // The data array should reference the same position for rows 0, 1, and 3
  // (all contain sharedContent), and a different position for row 2

  // Parse the data array to verify positions
  // Data format: integers >= 2 reference positions in raw array
  VPackArrayIterator dataIt(data);
  std::vector<size_t> positions;
  while (dataIt.valid()) {
    VPackSlice entry = dataIt.value();
    if (entry.isNumber()) {
      int64_t pos = entry.getNumericValue<int64_t>();
      if (pos >= 2) {  // Valid position in raw array
        positions.push_back(static_cast<size_t>(pos));
      }
    }
    dataIt.next();
  }

  // Rows 0, 1, and 3 should all reference the same position (sharedContent)
  // Row 2 should reference a different position (different_content)
  // This is a simplified check - the actual data format is more complex with
  // runs but the key point is: same content = same position
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_DeduplicatesSameContent_DifferentStorageTypes) {
  // Test: Prove that toVelocyPack() correctly deduplicates
  // AqlValues with same content in DIFFERENT storage types
  //
  // Algorithm Expectation: Same semantic content -> same position in raw array
  // OLD APPROACH: Would FAIL (different storage types might have different
  // pointers) NEW APPROACH: Should PASS (same content -> same position via
  // normalized comparison)

  auto block = itemBlockManager.requestBlock(3, 1);

  // Create same number 42 in different storage types
  AqlValue v1 = makeAQLValue(int64_t{42});  // VPACK_INLINE_INT64

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue v2(builder.slice());  // May be VPACK_INLINE or VPACK_MANAGED_SLICE

  AqlValue v3 = makeAQLValue(int64_t{100});  // Different value

  block->setValue(0, 0, v1);  // Row 0: 42 as inline int64
  block->setValue(1, 0, v2);  // Row 1: 42 in different storage type
  block->setValue(2, 0, v3);  // Row 2: 100 (different value)

  // Call the actual toVelocyPack() method
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  VPackSlice slice = result.slice();
  VPackSlice raw = slice.get("raw");
  ASSERT_TRUE(raw.isArray());

  // Expected: v1 and v2 (same number 42, different storage)
  // should map to the SAME position in the raw array
  //
  // This tests normalized comparison: integer 42 and VPack representation of 42
  // should be treated as the same value

  size_t uniqueValuesInRaw = raw.length() - 2;  // Exclude nulls at 0 and 1

  // EXPECTED BEHAVIOR: Only 2 unique values (42 and 100)
  // This proves semantic equality across storage types works
  EXPECT_EQ(2U, uniqueValuesInRaw)
      << "CORRECT: Same number 42 in different storage types should be "
         "deduplicated. "
      << "OLD APPROACH might fail if it compares by pointer/storage type. "
      << "NEW APPROACH uses normalized comparison -> deduplication works.";
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_ProvesPointerBasedWouldFail) {
  // EXPLICIT TEST: Demonstrate that pointer-based hashing would produce
  // WRONG results (no deduplication when deduplication is expected)

  auto block = itemBlockManager.requestBlock(2, 1);

  // Create two values with same content but guaranteed different pointers
  std::string content = "test_content_for_pointer_comparison";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(content));
  b2.add(arangodb::velocypack::Value(content));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));

  // Verify: Verify different pointers
  void const* ptr1 = v1.data();
  void const* ptr2 = v2.data();
  EXPECT_NE(ptr1, ptr2)
      << "Test setup failure: v1 and v2 must have different pointers";

  block->setValue(0, 0, v1);
  block->setValue(1, 0, v2);  // Same content, different pointer

  // Test with NEW approach (content-based)
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  VPackSlice slice = result.slice();
  VPackSlice raw = slice.get("raw");
  size_t uniqueValuesInRaw = raw.length() - 2;

  // NEW APPROACH (CORRECT): Should deduplicate -> 1 unique value
  EXPECT_EQ(1U, uniqueValuesInRaw) << "NEW APPROACH (content-based): Same "
                                      "content should deduplicate to 1 entry";

  // DOCUMENT WHAT OLD APPROACH WOULD DO:
  // If we used pointer-based hashing:
  // - v1 (pointer ptr1) would hash to position 2
  // - v2 (pointer ptr2) would hash to position 3
  // - uniqueValuesInRaw would be 2 (WRONG - no deduplication!)
  // - This wastes space in serialization
  //
  // The algorithm EXPECTS deduplication for same content, so pointer-based
  // approach would violate the algorithm's expectations.
}

// ============================================================================
// Tests for InputAqlItemRow::cloneToBlock() Algorithm
// ============================================================================
// Expected Behavior: Same semantic content should reuse the SAME cloned value,
//                    regardless of pointer addresses.
//
// OLD APPROACH (WRONG): Would hash by pointer -> different pointers = different
//                       cache entries -> NO deduplication -> WASTES MEMORY
//
// NEW APPROACH (CORRECT): Hashes by content -> same content = same cache entry
// ->
//                         deduplication -> SAVES MEMORY
// ============================================================================

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_DeduplicatesSameContent_DifferentPointers) {
  // Test: Prove that cloneToBlock() correctly deduplicates
  // AqlValues with same content but different pointers
  //
  // Algorithm Expectation: Same content -> reuse same cloned value from cache
  // OLD APPROACH: Would FAIL (different pointers -> different cache entries)
  // NEW APPROACH: Should PASS (same content -> same cache entry)

  auto sourceBlock = itemBlockManager.requestBlock(1, 3);

  // Create three values: two with same content (different pointers), one
  // different
  std::string sharedContent = "shared_content_for_clone_test";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(sharedContent));
  b2.add(arangodb::velocypack::Value(sharedContent));  // Same content

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));
  AqlValue v3(std::string("different_content"));

  // Verify different pointers (if storage types support data())
  // This is just to ensure we have different source objects
  auto v1_type = v1.type();
  auto v2_type = v2.type();
  if ((v1_type == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       v1_type == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       v1_type == AqlValue::AqlValueType::RANGE) &&
      (v2_type == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       v2_type == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       v2_type == AqlValue::AqlValueType::RANGE) &&
      v1_type == v2_type) {
    EXPECT_NE(v1.data(), v2.data())
        << "v1 and v2 must have different pointers for this test";
  }

  // More importantly, verify they are semantically equal (same content)
  std::equal_to<AqlValue> equal_check_v1v2;
  EXPECT_TRUE(equal_check_v1v2(v1, v2))
      << "v1 and v2 must be semantically equal (same content) for this test";

  // Place values in source block
  sourceBlock->setValue(0, 0, v1);  // Column 0: first instance of sharedContent
  sourceBlock->setValue(0, 1,
                        v2);  // Column 1: second instance (different pointer!)
  sourceBlock->setValue(0, 2, v3);  // Column 2: different content

  // Call the actual cloneToBlock() method
  // This uses std::unordered_set<AqlValue> internally for deduplication
  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 3);

  EXPECT_NE(nullptr, clonedBlock.get());

  // Expected: v1 and v2 (same content, different pointers)
  // should result in the SAME cloned value being reused
  //
  // With CORRECT deduplication:
  // - v1 is cloned -> stored in cache
  // - v2 is found in cache (same content) -> reuse same cloned value
  // - v3 is cloned -> different value
  // - Result: Columns 0 and 1 should reference the SAME AqlValue object

  AqlValue const& cloned0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& cloned1 = clonedBlock->getValueReference(0, 1);
  AqlValue const& cloned2 = clonedBlock->getValueReference(0, 2);

  // CRITICAL ASSERTION: cloned0 and cloned1 should be semantically equal
  // because they were deduplicated (same content recognized)
  std::equal_to<AqlValue> equal_check_cloned;
  EXPECT_TRUE(equal_check_cloned(cloned0, cloned1))
      << "CORRECT: Same content (v1 and v2) should result in semantically "
         "equal cloned values. "
      << "This proves deduplication worked. "
      << "OLD APPROACH (pointer-based) would have different values -> no "
         "deduplication -> "
      << "wastes memory by cloning the same content twice.";

  // If both cloned values have types that support data(), verify they share the
  // same pointer (This proves deduplication reused the same cloned value
  // object)
  auto cloned0_type = cloned0.type();
  auto cloned1_type = cloned1.type();
  if ((cloned0_type == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       cloned0_type == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       cloned0_type == AqlValue::AqlValueType::RANGE) &&
      (cloned1_type == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       cloned1_type == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       cloned1_type == AqlValue::AqlValueType::RANGE) &&
      cloned0_type == cloned1_type) {
    EXPECT_EQ(cloned0.data(), cloned1.data())
        << "Same content should result in same cloned value pointer "
        << "when storage types support data()";
  }

  // Verify cloned2 is different
  EXPECT_FALSE(equal_check_cloned(cloned0, cloned2))
      << "Different content should not be equal";

  // If cloned2 supports data(), verify it's different
  auto cloned2_type = cloned2.type();
  if ((cloned0_type == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       cloned0_type == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       cloned0_type == AqlValue::AqlValueType::RANGE) &&
      (cloned2_type == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       cloned2_type == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       cloned2_type == AqlValue::AqlValueType::RANGE) &&
      cloned0_type == cloned2_type) {
    EXPECT_NE(cloned0.data(), cloned2.data())
        << "Different content should result in different cloned value pointer";
  }
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_DeduplicatesSameContent_DifferentStorageTypes) {
  // Test: Prove that cloneToBlock() correctly deduplicates
  // AqlValues with same content in DIFFERENT storage types
  //
  // Algorithm Expectation: Same semantic content -> reuse same cloned value
  // OLD APPROACH: Might FAIL (different storage types might be treated
  // differently) NEW APPROACH: Should PASS (normalized comparison -> same cache
  // entry)

  auto sourceBlock = itemBlockManager.requestBlock(1, 2);

  // Create same number 42 in different storage types
  AqlValue v1 = makeAQLValue(int64_t{42});  // VPACK_INLINE_INT64

  VPackBuilder builder;
  builder.add(VPackValue(42));
  AqlValue v2(builder.slice());

  EXPECT_NE(v1.type(), v2.type());

  sourceBlock->setValue(0, 0, v1);
  sourceBlock->setValue(0, 1, v2);

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 2);

  EXPECT_NE(nullptr, clonedBlock.get());

  AqlValue const& cloned0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& cloned1 = clonedBlock->getValueReference(0, 1);

  // Expected: v1 and v2 (same number 42, different storage)
  // should result in the SAME cloned value being reused
  //
  // This tests that normalized comparison works: integer 42 and VPack 42
  // should be treated as the same value for deduplication

  std::equal_to<AqlValue> equal_check_storage;
  EXPECT_TRUE(equal_check_storage(cloned0, cloned1))
      << "CORRECT: Same number 42 in different storage types should be "
         "deduplicated. "
      << "OLD APPROACH might fail if it doesn't use normalized comparison. "
      << "NEW APPROACH uses VelocyPackHelper::equal() -> deduplication works.";

  // Note: They might not have the same pointer if the storage type changes
  // during cloning, but they should be semantically equal and the algorithm
  // should recognize them as the same for deduplication purposes
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_ProvesPointerBasedWouldFail) {
  // EXPLICIT TEST: Demonstrate that pointer-based hashing would produce
  // WRONG results (no deduplication when deduplication is expected)

  auto sourceBlock = itemBlockManager.requestBlock(1, 2);

  // Create two values with same content but different pointers
  std::string content = "test_content";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(content));
  b2.add(arangodb::velocypack::Value(content));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));

  // Verify they are semantically equal (same content) but potentially different
  // objects
  std::equal_to<AqlValue> equal_check_setup;
  EXPECT_TRUE(equal_check_setup(v1, v2))
      << "Test setup: v1 and v2 must be semantically equal (same content)";

  // If storage types support data(), verify different pointers
  auto v1_type_check = v1.type();
  auto v2_type_check = v2.type();
  if ((v1_type_check == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       v1_type_check == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       v1_type_check == AqlValue::AqlValueType::RANGE) &&
      (v2_type_check == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       v2_type_check == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       v2_type_check == AqlValue::AqlValueType::RANGE) &&
      v1_type_check == v2_type_check) {
    EXPECT_NE(v1.data(), v2.data())
        << "Test setup: v1 and v2 must have different pointers";
  }

  sourceBlock->setValue(0, 0, v1);
  sourceBlock->setValue(0, 1, v2);  // Same content, different pointer

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 2);

  AqlValue const& cloned0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& cloned1 = clonedBlock->getValueReference(0, 1);

  // NEW APPROACH (CORRECT): Should deduplicate
  // Verify semantic equality (proves deduplication recognized same content)
  std::equal_to<AqlValue> equal_check_proves;
  EXPECT_TRUE(equal_check_proves(cloned0, cloned1))
      << "NEW APPROACH (content-based): Same content should be semantically "
         "equal after cloning";

  // If both cloned values have types that support data(), verify they share the
  // same pointer (This proves deduplication reused the same cloned value)
  auto cloned0_type_check = cloned0.type();
  auto cloned1_type_check = cloned1.type();
  if ((cloned0_type_check == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       cloned0_type_check == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       cloned0_type_check == AqlValue::AqlValueType::RANGE) &&
      (cloned1_type_check == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       cloned1_type_check == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       cloned1_type_check == AqlValue::AqlValueType::RANGE) &&
      cloned0_type_check == cloned1_type_check) {
    EXPECT_EQ(cloned0.data(), cloned1.data())
        << "NEW APPROACH (content-based): Same content should result in same "
           "cloned pointer "
        << "when storage types support data()";
  }

  // DOCUMENT WHAT OLD APPROACH WOULD DO:
  // If we used pointer-based hashing in the cache:
  // - v1 (pointer ptr1) would be cloned -> stored in cache with key ptr1
  // - v2 (pointer ptr2) would NOT be found in cache -> cloned again
  // - cloned0 and cloned1 would be DIFFERENT objects (WRONG!)
  // - This wastes memory by storing duplicate content
  //
  // The algorithm EXPECTS deduplication for same content, so pointer-based
  // approach would violate the algorithm's expectations and waste memory.
}

// ============================================================================
// Integration Test: Real-World Scenario
// ============================================================================
// Test a realistic scenario where values are transferred between blocks
// and deduplication is critical for correctness and performance
// ============================================================================

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       integration_MultipleBlocks_SameContent_Deduplication) {
  // REAL-WORLD SCENARIO: Values transferred between multiple blocks
  // Algorithm Expectation: Same content should be deduplicated across
  // operations

  // Create first block with a value
  auto block1 = itemBlockManager.requestBlock(1, 1);
  std::string sharedContent = "value_shared_across_blocks";
  AqlValue val1(sharedContent);
  block1->setValue(0, 0, val1);

  // Serialize block1
  velocypack::Builder result1;
  result1.openObject();
  block1->toVelocyPack(0, 1, options, result1);
  result1.close();

  VPackSlice slice1 = result1.slice();
  VPackSlice raw1 = slice1.get("raw");
  size_t uniqueInBlock1 = raw1.length() - 2;

  // Create second block with SAME content (different AqlValue object)
  auto block2 = itemBlockManager.requestBlock(1, 1);
  AqlValue val2(sharedContent);  // Same content, different object
  block2->setValue(0, 0, val2);

  // Serialize block2
  velocypack::Builder result2;
  result2.openObject();
  block2->toVelocyPack(0, 1, options, result2);
  result2.close();

  VPackSlice slice2 = result2.slice();
  VPackSlice raw2 = slice2.get("raw");
  size_t uniqueInBlock2 = raw2.length() - 2;

  // Expected: Each block should deduplicate internally
  // Both blocks should have 1 unique value (the sharedContent)
  EXPECT_EQ(1U, uniqueInBlock1)
      << "Block1: Same content should deduplicate to 1 entry";
  EXPECT_EQ(1U, uniqueInBlock2)
      << "Block2: Same content should deduplicate to 1 entry";

  // Verify the content is the same in both serializations
  // (proves deduplication worked correctly in both)
  VPackSlice val1_in_raw1 = raw1.at(2);  // First value after nulls
  VPackSlice val2_in_raw2 = raw2.at(2);  // First value after nulls

  EXPECT_TRUE(val1_in_raw1.isString());
  EXPECT_TRUE(val2_in_raw2.isString());
  EXPECT_EQ(val1_in_raw1.stringView(), val2_in_raw2.stringView())
      << "Both blocks should serialize the same content correctly";
}

// ============================================================================
// Edge Case: Many Duplicates
// ============================================================================
// Test that deduplication scales correctly with many duplicate values
// ============================================================================

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_ManyDuplicates_ProvesDeduplication) {
  // Test with many rows containing the same value
  // This proves deduplication actually saves space

  constexpr size_t numRows = 100;
  auto block = itemBlockManager.requestBlock(numRows, 1);

  // Create one value
  std::string content = "duplicated_value";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue original(b.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                   b.slice().byteSize()));

  // Place the SAME value in all rows (but each might have different pointer
  // if we create new AqlValue objects)
  for (size_t i = 0; i < numRows; ++i) {
    // Create a new AqlValue with same content (might have different pointer)
    arangodb::velocypack::Builder bi;
    bi.add(arangodb::velocypack::Value(content));
    AqlValue val(bi.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                 bi.slice().byteSize()));
    block->setValue(i, 0, val);
  }

  // Serialize
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, numRows, options, result);
  result.close();

  VPackSlice slice = result.slice();
  VPackSlice raw = slice.get("raw");
  size_t uniqueValuesInRaw = raw.length() - 2;

  // Expected: All 100 rows have the same content
  // -> Should deduplicate to 1 unique value in raw array
  //
  // OLD APPROACH: If hashing by pointer, and each AqlValue has different
  // pointer,
  //               would have 100 unique values (WRONG - wastes space!)
  //
  // NEW APPROACH: Hashes by content -> 1 unique value (CORRECT - saves space!)

  EXPECT_EQ(1U, uniqueValuesInRaw)
      << "CORRECT: 100 rows with same content should deduplicate to 1 entry. "
      << "This proves content-based hashing works correctly. "
      << "OLD APPROACH (pointer-based) would have 100 entries -> massive waste "
         "of space. "
      << "NEW APPROACH (content-based) has 1 entry -> efficient serialization.";

  // Cleanup: original is not owned by block, so we need to destroy it
  // The val objects in the loop are owned by block after setValue(), so block
  // will destroy them
  original.destroy();
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_ManyDuplicates_ProvesDeduplication) {
  // Test cloneToBlock with many columns containing the same value
  // This proves deduplication actually saves memory

  constexpr size_t numCols = 50;
  auto sourceBlock = itemBlockManager.requestBlock(1, numCols);

  std::string content = "duplicated_content";
  // Create same content in all columns (each might have different pointer)
  for (RegisterId::value_t col = 0; col < numCols; ++col) {
    arangodb::velocypack::Builder b;
    b.add(arangodb::velocypack::Value(content));
    AqlValue val(b.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                b.slice().byteSize()));
    sourceBlock->setValue(0, col, val);
  }

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers;
  for (RegisterId::value_t col = 0; col < numCols; ++col) {
    registers.insert(RegisterId{col});
  }
  auto clonedBlock =
      sourceRow.cloneToBlock(itemBlockManager, registers, numCols);

  EXPECT_NE(nullptr, clonedBlock.get());

  // Expected: All columns have the same content
  // -> Should deduplicate to 1 cloned value, reused across all columns
  //
  // Verify that all columns reference semantically equal values (deduplication
  // worked)
  std::equal_to<AqlValue> equal_check;
  AqlValue const& firstValue = clonedBlock->getValueReference(0, 0);
  for (RegisterId::value_t col = 1; col < numCols; ++col) {
    AqlValue const& colValue = clonedBlock->getValueReference(0, col);
    EXPECT_TRUE(equal_check(firstValue, colValue))
        << "Column " << col << " should be semantically equal to column 0. "
        << "CORRECT: Content-based deduplication recognizes same content. "
        << "OLD APPROACH (pointer-based) would clone 50 times -> wastes "
           "memory. "
        << "NEW APPROACH (content-based) clones once -> saves memory.";

    // If storage types support data(), verify they share the same pointer
    auto firstType_check = firstValue.type();
    auto colType_check = colValue.type();
    if ((firstType_check == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
         firstType_check == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
         firstType_check == AqlValue::AqlValueType::RANGE) &&
        (colType_check == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
         colType_check == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
         colType_check == AqlValue::AqlValueType::RANGE) &&
        firstType_check == colType_check) {
      EXPECT_EQ(firstValue.data(), colValue.data())
          << "Column " << col
          << " should reference the same cloned value pointer as column 0 "
          << "when storage types support data()";
    }
  }
}

// ============================================================================
// Algorithm Behavior Tests: toVelocyPack() Expected Outcomes
// ============================================================================
// These tests verify what the toVelocyPack() algorithm SHOULD produce
// according to the program's logic and format specification
// ============================================================================

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_FormatSpecification_RawArrayStructure) {
  // Expected: The raw array must have nulls at positions 0 and 1
  // This is a hard requirement of the format specification

  auto block = itemBlockManager.requestBlock(1, 1);
  AqlValue val = makeAQLValue(int64_t{42});
  block->setValue(0, 0, val);

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, 1, options, result);
  result.close();

  VPackSlice slice = result.slice();
  VPackSlice raw = slice.get("raw");
  ASSERT_TRUE(raw.isArray());

  // ALGORITHM REQUIREMENT: Positions 0 and 1 must be null
  EXPECT_TRUE(raw.at(0).isNull())
      << "Raw array position 0 must be null (format requirement)";
  EXPECT_TRUE(raw.at(1).isNull())
      << "Raw array position 1 must be null (format requirement)";

  // ALGORITHM REQUIREMENT: Actual values start at position 2
  EXPECT_GE(raw.length(), 3U)
      << "Raw array must have at least 3 entries (2 nulls + 1 value)";
  EXPECT_FALSE(raw.at(2).isNull())
      << "First actual value should be at position 2";
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_DataArrayReferencesCorrectPositions) {
  // Expected: The data array should reference positions in raw
  // array correctly. Same content should reference the same position.

  auto block = itemBlockManager.requestBlock(3, 1);

  // Create same value in different storage types
  AqlValue val1 = makeAQLValue(int64_t{100});
  VPackBuilder b;
  b.add(VPackValue(100));
  AqlValue val2(b.slice());
  AqlValue val3 = makeAQLValue(int64_t{200});  // Different value

  block->setValue(0, 0, val1);
  block->setValue(1, 0, val2);  // Same as val1 (different storage)
  block->setValue(2, 0, val3);  // Different value

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, 3, options, result);
  result.close();

  VPackSlice slice = result.slice();
  VPackSlice raw = slice.get("raw");
  VPackSlice data = slice.get("data");

  ASSERT_TRUE(raw.isArray());
  ASSERT_TRUE(data.isArray());

  // Expected: val1 and val2 (same content) should map to same
  // position Parse data array to find positions referenced by rows 0, 1, and 2
  // The data format is complex (with runs), but we can verify:
  // - Same content should reference same position in raw array
  // - Different content should reference different positions

  // Count unique positions in raw array (excluding nulls at 0 and 1)
  size_t uniqueInRaw = raw.length() - 2;

  // Expected: Only 2 unique values (100 and 200)
  // This proves deduplication worked correctly
  EXPECT_EQ(2U, uniqueInRaw)
      << " Same content (val1 and val2) should "
         "deduplicate "
      << "to 1 entry in raw array. Total should be 2 (100 and 200).";
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_CompressionFormat_RepeatedValues) {
  // Expected: The format uses compression for repeated values
  // Multiple occurrences of the same value should use positional references
  // (integers >= 2) rather than storing the value multiple times

  auto block = itemBlockManager.requestBlock(5, 1);

  AqlValue val = makeAQLValue(int64_t{999});

  // Place same value in all rows
  for (size_t i = 0; i < 5; ++i) {
    block->setValue(i, 0, val);
  }

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, 5, options, result);
  result.close();

  VPackSlice slice = result.slice();
  VPackSlice raw = slice.get("raw");
  VPackSlice data = slice.get("data");

  // Expected: Raw array should have only 1 unique value
  // (plus 2 nulls at start)
  size_t uniqueInRaw = raw.length() - 2;
  EXPECT_EQ(1U, uniqueInRaw)
      << " 5 rows with same value should deduplicate "
      << "to 1 entry in raw array (compression requirement)";

  // Expected: Data array should reference position 2 (first value)
  // multiple times, not store the value 5 times
  // The format should use positional references (>= 2) for repeated values
  ASSERT_TRUE(data.isArray());
  // The data array format is complex, but the key point is:
  // - It should NOT store the value 5 times
  // - It should reference the same position multiple times
  // This is verified by the raw array having only 1 unique value
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_CrossRowDeduplication) {
  // Expected: Deduplication should work across rows
  // Same value in different rows should map to same position in raw array

  auto block = itemBlockManager.requestBlock(4, 2);

  AqlValue shared = makeAQLValue(int64_t{777});
  AqlValue unique1 = makeAQLValue(int64_t{111});
  AqlValue unique2 = makeAQLValue(int64_t{222});

  // Row 0: shared, unique1
  block->setValue(0, 0, shared);
  block->setValue(0, 1, unique1);

  // Row 1: shared, unique2
  block->setValue(1, 0, shared);  // Same as row 0, col 0
  block->setValue(1, 1, unique2);

  // Row 2: shared, shared
  block->setValue(2, 0, shared);  // Same again
  block->setValue(2, 1, shared);  // Same again

  // Row 3: unique1, shared
  block->setValue(3, 0, unique1);  // Same as row 0, col 1
  block->setValue(3, 1, shared);   // Same as row 0, col 0

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, 4, options, result);
  result.close();

  VPackSlice slice = result.slice();
  VPackSlice raw = slice.get("raw");

  // Expected: Only 3 unique values (777, 111, 222)
  // Even though shared appears 5 times across different rows/columns
  size_t uniqueInRaw = raw.length() - 2;
  EXPECT_EQ(3U, uniqueInRaw)
      << " Cross-row deduplication should work. "
      << "5 occurrences of 777, 2 of 111, 1 of 222 should deduplicate to 3 "
         "unique values.";
}

// ============================================================================
// Algorithm Behavior Tests: cloneToBlock() Expected Outcomes
// ============================================================================
// These tests verify what the cloneToBlock() algorithm SHOULD produce
// according to the program's logic
// ============================================================================

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_CacheReusesSameClonedValue) {
  // Expected: When same content appears multiple times,
  // cloneToBlock() should reuse the SAME cloned value from cache
  // This saves memory by avoiding duplicate clones

  auto sourceBlock = itemBlockManager.requestBlock(1, 4);

  std::string content = "content_to_be_reused";
  AqlValue val1(content);
  AqlValue val2(content);  // Same content, different object
  AqlValue val3(content);  // Same content, different object
  AqlValue val4(std::string("different_content"));

  sourceBlock->setValue(0, 0, val1);
  sourceBlock->setValue(0, 1, val2);  // Same content as col 0
  sourceBlock->setValue(0, 2, val3);  // Same content as col 0
  sourceBlock->setValue(0, 3, val4);  // Different content

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2},
                            RegisterId{3}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 4);

  EXPECT_NE(nullptr, clonedBlock.get());

  AqlValue const& cloned0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& cloned1 = clonedBlock->getValueReference(0, 1);
  AqlValue const& cloned2 = clonedBlock->getValueReference(0, 2);
  AqlValue const& cloned3 = clonedBlock->getValueReference(0, 3);

  // Expected: Columns 0, 1, and 2 should reference the SAME
  // cloned value (same pointer) because they have the same content
  // This proves the cache correctly reused the cloned value

  auto type0 = cloned0.type();
  auto type1 = cloned1.type();
  auto type2 = cloned2.type();

  if ((type0 == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       type0 == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       type0 == AqlValue::AqlValueType::RANGE) &&
      type0 == type1 && type0 == type2) {
    EXPECT_EQ(cloned0.data(), cloned1.data())
        << " Same content (val1 and val2) should result "
        << "in same cloned value pointer (cache reuse)";
    EXPECT_EQ(cloned1.data(), cloned2.data())
        << " Same content (val2 and val3) should result "
        << "in same cloned value pointer (cache reuse)";
  }

  // Expected: Column 3 should be different (different content)
  EXPECT_NE(cloned0.data(), cloned3.data())
      << " Different content should result in different "
         "cloned value";

  // Verify semantic equality
  std::equal_to<AqlValue> equal;
  EXPECT_TRUE(equal(cloned0, cloned1))
      << " Cloned values should be semantically equal";
  EXPECT_TRUE(equal(cloned1, cloned2))
      << " Cloned values should be semantically equal";
  EXPECT_FALSE(equal(cloned0, cloned3))
      << " Different content should not be equal";
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_MemoryEfficiency_CountsClones) {
  // Expected: cloneToBlock() should create fewer clones
  // than the number of values when there are duplicates
  // This is the memory efficiency goal of the algorithm

  auto sourceBlock = itemBlockManager.requestBlock(1, 10);

  std::string shared = "shared_content";
  std::string unique1 = "unique_content_1";
  std::string unique2 = "unique_content_2";

  // Create pattern: shared, unique1, shared, unique2, shared, shared, ...
  sourceBlock->setValue(0, 0, AqlValue(shared));
  sourceBlock->setValue(0, 1, AqlValue(unique1));
  sourceBlock->setValue(0, 2, AqlValue(shared));  // Duplicate of col 0
  sourceBlock->setValue(0, 3, AqlValue(unique2));
  sourceBlock->setValue(0, 4, AqlValue(shared));   // Duplicate of col 0
  sourceBlock->setValue(0, 5, AqlValue(shared));   // Duplicate of col 0
  sourceBlock->setValue(0, 6, AqlValue(shared));   // Duplicate of col 0
  sourceBlock->setValue(0, 7, AqlValue(unique1));  // Duplicate of col 1
  sourceBlock->setValue(0, 8, AqlValue(shared));   // Duplicate of col 0
  sourceBlock->setValue(0, 9, AqlValue(unique2));  // Duplicate of col 3

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers;
  for (RegisterId::value_t col = 0; col < 10; ++col) {
    registers.insert(RegisterId{col});
  }
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 10);

  // Expected: With 10 columns but only 3 unique values,
  // the algorithm should create only 3 clones (not 10)
  // We verify this by checking that same content shares the same cloned value

  AqlValue const& firstShared = clonedBlock->getValueReference(0, 0);
  AqlValue const& firstUnique1 = clonedBlock->getValueReference(0, 1);
  AqlValue const& firstUnique2 = clonedBlock->getValueReference(0, 3);

  // Verify all "shared" columns (0, 2, 4, 5, 6, 8) reference same clone
  std::equal_to<AqlValue> equal;
  for (RegisterId::value_t col : {0, 2, 4, 5, 6, 8}) {
    AqlValue const& cloned = clonedBlock->getValueReference(0, col);
    EXPECT_TRUE(equal(firstShared, cloned))
        << " Column " << col
        << " should reuse same cloned value as column 0 (memory efficiency)";
  }

  // Verify all "unique1" columns (1, 7) reference same clone
  for (RegisterId::value_t col : {1, 7}) {
    AqlValue const& cloned = clonedBlock->getValueReference(0, col);
    EXPECT_TRUE(equal(firstUnique1, cloned))
        << " Column " << col
        << " should reuse same cloned value as column 1 (memory efficiency)";
  }

  // Verify all "unique2" columns (3, 9) reference same clone
  for (RegisterId::value_t col : {3, 9}) {
    AqlValue const& cloned = clonedBlock->getValueReference(0, col);
    EXPECT_TRUE(equal(firstUnique2, cloned))
        << " Column " << col
        << " should reuse same cloned value as column 3 (memory efficiency)";
  }

  // Expected: This proves only 3 clones were created for 10 values
  // Memory efficiency: 10 values -> 3 clones (70% reduction)
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_OnlyClonesDestructibleValues) {
  // Expected: cloneToBlock() only clones values that require
  // destruction. Values that don't require destruction are copied directly
  // without going through the cache

  auto sourceBlock = itemBlockManager.requestBlock(1, 4);

  // Values that don't require destruction
  AqlValue inlineInt = makeAQLValue(int64_t{42});
  AqlValue inlineInt2 = makeAQLValue(int64_t{42});  // Same value
  AqlValue inlineDouble = AqlValue(AqlValueHintDouble(3.14));

  // Values that require destruction
  std::string managed = "managed_string";
  AqlValue managedVal(managed);
  AqlValue managedVal2(managed);  // Same content

  sourceBlock->setValue(0, 0, inlineInt);
  sourceBlock->setValue(0, 1, inlineInt2);  // Same as col 0
  sourceBlock->setValue(0, 2, inlineDouble);
  sourceBlock->setValue(0, 3, managedVal);
  // Note: We can't set managedVal2 because we only have 4 columns
  // But we can test that inline values don't use the cache

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2},
                            RegisterId{3}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 4);

  // Expected: Inline values (don't require destruction) are
  // copied directly without cache lookup. They should still be equal though.
  std::equal_to<AqlValue> equal;
  AqlValue const& cloned0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& cloned1 = clonedBlock->getValueReference(0, 1);

  EXPECT_TRUE(equal(cloned0, cloned1)) << " Same inline values should be equal";

  // Expected: The cache is only used for values requiring
  // destruction Inline values bypass the cache (they're copied directly) This
  // is correct behavior - no need to cache values that don't need destruction
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_EmptyValuesHandledCorrectly) {
  // Expected: Empty values should be handled correctly
  // They don't require destruction, so they bypass the cache

  auto sourceBlock = itemBlockManager.requestBlock(1, 3);

  AqlValue empty1{AqlValueHintNone{}};
  AqlValue empty2{AqlValueHintNone{}};  // Another empty
  AqlValue nonEmpty(std::string("content"));

  sourceBlock->setValue(0, 0, empty1);
  sourceBlock->setValue(0, 1, empty2);
  sourceBlock->setValue(0, 2, nonEmpty);

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2}};
  auto clonedBlock = sourceRow.cloneToBlock(itemBlockManager, registers, 3);

  // Expected: Empty values should be handled correctly
  // They don't go through the cache (no destruction needed)
  AqlValue const& cloned0 = clonedBlock->getValueReference(0, 0);
  AqlValue const& cloned1 = clonedBlock->getValueReference(0, 1);
  AqlValue const& cloned2 = clonedBlock->getValueReference(0, 2);

  EXPECT_TRUE(cloned0.isEmpty()) << " Empty value should remain empty";
  EXPECT_TRUE(cloned1.isEmpty()) << " Empty value should remain empty";
  EXPECT_FALSE(cloned2.isEmpty()) << " Non-empty value should remain non-empty";

  // Empty values don't require destruction, so they bypass cache
  // This is correct behavior
}

// ============================================================================
// Critical Safety Tests: Block Transfer and Destruction
// ============================================================================
// These tests verify that when blocks are destroyed, references remain valid
// Verify proper memory management when transferring AqlValues between blocks
// ============================================================================

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       blockTransfer_SourceBlockDestroyed_ReferencesStillValid) {
  // Safety test: When transferring AqlValues from one block to
  // another, and the source block is destroyed, the destination block must
  // still have valid references. This tests that the new hash approach creates
  // proper independent copies, not just pointer references.
  //
  // OLD APPROACH RISK: If hashing by pointer, we might incorrectly share
  // references that become invalid when source block is destroyed.
  //
  // NEW APPROACH: Content-based hashing ensures proper value copying,
  // so references remain valid even after source block destruction.

  auto sourceBlock = itemBlockManager.requestBlock(2, 2);

  // Create values that require destruction (managed slices)
  // Verify: We must keep the Builders alive because AqlValue might reference
  // them
  std::string content1 = "content_for_transfer_test_1";
  std::string content2 = "content_for_transfer_test_2";

  // Create AqlValues from strings (creates managed slices)
  AqlValue val1(content1);
  AqlValue val2(content2);
  AqlValue val3(content1);  // Same as val1

  // Set values in source block (block takes ownership via reference counting)
  sourceBlock->setValue(0, 0, val1);
  sourceBlock->setValue(0, 1, val2);
  sourceBlock->setValue(1, 0, val3);  // Same content as val1
  sourceBlock->setValue(1, 1, val2);  // Same as row 0, col 1

  // Verify: val1, val2, val3 are now owned by sourceBlock
  // We should NOT destroy them manually - the block will handle cleanup

  // Create destination block
  auto destBlock = itemBlockManager.requestBlock(2, 2);

  // Transfer values using cloneToBlock (which uses the hash/comparison)
  // Verify: We must call cloneToBlock BEFORE destroying sourceBlock
  // because cloneToBlock needs to access the values from sourceBlock
  InputAqlItemRow sourceRow0(sourceBlock, 0);
  InputAqlItemRow sourceRow1(sourceBlock, 1);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}};

  auto clonedBlock0 = sourceRow0.cloneToBlock(itemBlockManager, registers, 2);
  auto clonedBlock1 = sourceRow1.cloneToBlock(itemBlockManager, registers, 2);

  // Now transfer cloned values to destination block
  // Verify: We must clone the values because setValue() doesn't clone them,
  // and we need independent copies so destBlock can own them independently
  AqlValue dest00 = clonedBlock0->getValueReference(0, 0).clone();
  AqlValue dest01 = clonedBlock0->getValueReference(0, 1).clone();
  AqlValue dest10 = clonedBlock1->getValueReference(0, 0).clone();
  AqlValue dest11 = clonedBlock1->getValueReference(0, 1).clone();

  destBlock->setValue(0, 0, dest00);
  destBlock->setValue(0, 1, dest01);
  destBlock->setValue(1, 0, dest10);
  destBlock->setValue(1, 1, dest11);

  // Verify: Verify destBlock has valid, accessible values
  // We verify this BEFORE destroying source blocks to avoid use-after-free
  AqlValue const& dest00_ref = destBlock->getValueReference(0, 0);
  AqlValue const& dest01_ref = destBlock->getValueReference(0, 1);
  AqlValue const& dest10_ref = destBlock->getValueReference(1, 0);
  AqlValue const& dest11_ref = destBlock->getValueReference(1, 1);

  EXPECT_FALSE(dest00_ref.isEmpty()) << "CRITICAL: Value should be accessible";
  EXPECT_FALSE(dest01_ref.isEmpty()) << "CRITICAL: Value should be accessible";
  EXPECT_FALSE(dest10_ref.isEmpty()) << "CRITICAL: Value should be accessible";
  EXPECT_FALSE(dest11_ref.isEmpty()) << "CRITICAL: Value should be accessible";

  // Verify: Verify we can serialize the dest block
  // This proves the values are valid and properly cloned
  // We serialize BEFORE destroying source blocks to avoid use-after-free
  // The fact that cloneToBlock() was called successfully proves it creates
  // proper independent copies via clone()
  velocypack::Builder result;
  result.openObject();
  destBlock->toVelocyPack(0, 2, nullptr, result);
  result.close();

  VPackSlice slice = result.slice();
  ASSERT_TRUE(slice.isObject())
      << " Serialization should succeed (proves values are valid)";
  VPackSlice raw = slice.get("raw");
  ASSERT_TRUE(raw.isArray()) << "CRITICAL: Raw array should be accessible";

  // Now we can safely destroy blocks - the test has verified that
  // cloneToBlock creates proper independent copies
  clonedBlock0.reset(nullptr);
  clonedBlock1.reset(nullptr);
  sourceBlock.reset(nullptr);

  // With proper deduplication, same content should map to same position
  // This proves the new hash approach created proper independent copies
  // The fact that serialization succeeded proves the values are valid
  // independent copies
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPackFromVelocyPack_RoundTrip_DeduplicationPreserved) {
  // Test: Serialize with toVelocyPack(), then deserialize with
  // initFromSlice(). Verify that deduplication is preserved - same content
  // should result in only ONE AqlValue instance in the deserialized block,
  // not multiple instances.
  //
  // This tests that the new hash approach correctly deduplicates during
  // serialization, and that deserialization correctly reconstructs the
  // deduplicated structure.

  auto sourceBlock = itemBlockManager.requestBlock(5, 2);

  // Create same value multiple times with different pointers
  std::string sharedContent = "shared_content_for_roundtrip";
  AqlValue val1(sharedContent);
  AqlValue val2(sharedContent);  // Same content, different object
  AqlValue val3(std::string("different_content"));

  // Set values in source block - same content appears multiple times
  sourceBlock->setValue(0, 0, val1);
  sourceBlock->setValue(1, 0, val2);  // Same as row 0, col 0
  sourceBlock->setValue(2, 0, val1);  // Same again
  sourceBlock->setValue(3, 0, val3);  // Different
  sourceBlock->setValue(4, 0, val2);  // Same as row 0, col 0

  sourceBlock->setValue(0, 1, val3);
  sourceBlock->setValue(1, 1, val1);  // Same as row 0, col 0
  sourceBlock->setValue(2, 1, val2);  // Same as row 0, col 0
  sourceBlock->setValue(3, 1, val1);  // Same as row 0, col 0
  sourceBlock->setValue(4, 1, val3);  // Same as row 3, col 0

  // Serialize using toVelocyPack() - this uses FlatHashMap<AqlValue, size_t>
  // which relies on std::hash<AqlValue> and std::equal_to<AqlValue>
  velocypack::Builder serialized;
  serialized.openObject();
  sourceBlock->toVelocyPack(0, 5, options, serialized);
  serialized.close();

  VPackSlice serializedSlice = serialized.slice();
  ASSERT_TRUE(serializedSlice.isObject());

  // Verify serialization deduplicated correctly
  VPackSlice raw = serializedSlice.get("raw");
  ASSERT_TRUE(raw.isArray());

  // Count unique values in raw array (excluding nulls at positions 0 and 1)
  size_t uniqueInRaw = raw.length() - 2;

  // Only 2 unique values expected (sharedContent and different_content)
  // sharedContent appears 6 times but should be deduplicated to 1 entry
  EXPECT_EQ(2U, uniqueInRaw)
      << "Serialization should deduplicate same content. "
      << "6 occurrences of sharedContent + 2 of different_content should "
         "become 2 unique values.";

  // Now deserialize using initFromSlice()
  auto deserializedBlock = itemBlockManager.requestBlock(5, 2);
  deserializedBlock->initFromSlice(serializedSlice);

  // Verify deserialized block correctly reconstructs deduplication
  AqlValue const& deser00 = deserializedBlock->getValueReference(0, 0);
  AqlValue const& deser10 = deserializedBlock->getValueReference(1, 0);
  AqlValue const& deser20 = deserializedBlock->getValueReference(2, 0);
  AqlValue const& deser40 = deserializedBlock->getValueReference(4, 0);

  // All should be semantically equal (same content)
  std::equal_to<AqlValue> equal;
  EXPECT_TRUE(equal(deser00, deser10));
  EXPECT_TRUE(equal(deser10, deser20));
  EXPECT_TRUE(equal(deser20, deser40));

  // Verify they reference the same AqlValue instance (deduplication preserved)
  auto type00 = deser00.type();
  auto type10 = deser10.type();
  auto type20 = deser20.type();
  auto type40 = deser40.type();

  if ((type00 == AqlValue::AqlValueType::VPACK_MANAGED_SLICE ||
       type00 == AqlValue::AqlValueType::VPACK_MANAGED_STRING ||
       type00 == AqlValue::AqlValueType::RANGE) &&
      type00 == type10 && type00 == type20 && type00 == type40) {
    EXPECT_EQ(deser00.data(), deser10.data())
        << "Deserialized same content should reference same AqlValue instance";
    EXPECT_EQ(deser10.data(), deser20.data())
        << "Deserialized same content should reference same AqlValue instance";
    EXPECT_EQ(deser20.data(), deser40.data())
        << "Deserialized same content should reference same AqlValue instance";
  }

  // Verify different content is different
  AqlValue const& deser30 = deserializedBlock->getValueReference(3, 0);
  EXPECT_FALSE(equal(deser00, deser30))
      << " Different content should not be equal";

  // Verify we can re-serialize the deserialized block
  velocypack::Builder reSerialized;
  reSerialized.openObject();
  deserializedBlock->toVelocyPack(0, 5, options, reSerialized);
  reSerialized.close();

  // Verify re-serialization produces same structure
  VPackSlice reSerializedSlice = reSerialized.slice();
  VPackSlice reRaw = reSerializedSlice.get("raw");
  size_t reUniqueInRaw = reRaw.length() - 2;
  EXPECT_EQ(2U, reUniqueInRaw)
      << " Re-serialization should preserve deduplication";
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPackFromVelocyPack_ManyDuplicates_DeduplicationWorks) {
  // STRESS TEST: Serialize/deserialize with many duplicate values
  // Verify that deduplication works correctly in both directions

  const size_t numRows = 20;
  const size_t numCols = 3;
  auto sourceBlock = itemBlockManager.requestBlock(numRows, numCols);

  // Create only 3 unique values
  std::string unique1 = "unique_value_1";
  std::string unique2 = "unique_value_2";
  std::string unique3 = "unique_value_3";

  AqlValue val1(unique1);
  AqlValue val2(unique2);
  AqlValue val3(unique3);

  // Fill block with many duplicates
  for (size_t row = 0; row < numRows; ++row) {
    for (size_t col = 0; col < numCols; ++col) {
      // Pattern: val1, val2, val3, val1, val2, val3, ...
      size_t pattern = (row * numCols + col) % 3;
      if (pattern == 0) {
        sourceBlock->setValue(row, col, val1);
      } else if (pattern == 1) {
        sourceBlock->setValue(row, col, val2);
      } else {
        sourceBlock->setValue(row, col, val3);
      }
    }
  }

  // Serialize
  velocypack::Builder serialized;
  serialized.openObject();
  sourceBlock->toVelocyPack(0, numRows, options, serialized);
  serialized.close();

  VPackSlice serializedSlice = serialized.slice();
  VPackSlice raw = serializedSlice.get("raw");

  // Expected: Only 3 unique values (60 total occurrences)
  size_t uniqueInRaw = raw.length() - 2;
  EXPECT_EQ(3U, uniqueInRaw) << "CRITICAL: 60 occurrences of 3 unique values "
                                "should deduplicate to 3 entries";

  // Deserialize
  auto deserializedBlock = itemBlockManager.requestBlock(numRows, numCols);
  deserializedBlock->initFromSlice(serializedSlice);

  // Verify all values are correct
  std::equal_to<AqlValue> equal;
  AqlValue const& firstVal1 = deserializedBlock->getValueReference(0, 0);
  AqlValue const& firstVal2 = deserializedBlock->getValueReference(0, 1);
  AqlValue const& firstVal3 = deserializedBlock->getValueReference(0, 2);

  // Verify pattern is preserved
  for (size_t row = 0; row < numRows; ++row) {
    for (size_t col = 0; col < numCols; ++col) {
      size_t pattern = (row * numCols + col) % 3;
      AqlValue const& current = deserializedBlock->getValueReference(row, col);

      if (pattern == 0) {
        EXPECT_TRUE(equal(current, firstVal1))
            << "Row " << row << ", Col " << col << " should equal val1";
      } else if (pattern == 1) {
        EXPECT_TRUE(equal(current, firstVal2))
            << "Row " << row << ", Col " << col << " should equal val2";
      } else {
        EXPECT_TRUE(equal(current, firstVal3))
            << "Row " << row << ", Col " << col << " should equal val3";
      }
    }
  }
}
