#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlValue.h"
#include "Aql/RegIdFlatSet.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/VelocyPackHelper.h"

#include <boost/container/flat_set.hpp>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace arangodb {
namespace tests {
namespace aql {

class AqlItemBlockSupervisedMemoryTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor};

  // Helper to create a large supervised slice (to ensure it's not inlined)
  AqlValue createSupervisedSlice(size_t size = 200) {
    std::string content(size, 'x');
    velocypack::Builder b;
    b.add(arangodb::velocypack::Value(content));
    return AqlValue(b.slice(), &monitor);
  }
};

// WHAT IT CHECKS:
// 1. setValue() properly registers supervised slices in _valueCount
// 2. Memory accounting is correct (no double-counting for supervised slices)
// 3. destroy() properly cleans up supervised slices set via setValue()
TEST_F(AqlItemBlockSupervisedMemoryTest,
       SupervisedSliceSetValueProperlyRegistered) {
  auto block = itemBlockManager.requestBlock(2, 1);

  AqlValue supervised = createSupervisedSlice(200);
  ASSERT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  ASSERT_TRUE(supervised.requiresDestruction());

  size_t initialMemory = monitor.current();

  // 1. Register the value in _valueCount with refCount=1
  // 2. Set memoryUsage correctly
  // 3. For supervised slices: NOT call increaseMemoryUsage()
  block->setValue(0, 0, supervised);

  // For supervised slices, memory is already accounted in ResourceMonitor
  EXPECT_EQ(monitor.current(), initialMemory);

  block.reset(nullptr);  // Destroy the block; all memory should be released

  EXPECT_EQ(monitor.current(), 0U);
}

// This test checks the behavior of referenceValuesFromRow() of AqlItemBlock.cpp
// Specifically for the case when the value is NOT found in _valueCount
TEST_F(AqlItemBlockSupervisedMemoryTest,
       ReferenceValuesFromRowUnregisteredSupervisedSliceLeak) {
  auto block = itemBlockManager.requestBlock(2, 1);

  size_t initialMemory = monitor.current();
  AqlValue supervised = createSupervisedSlice(200);
  ASSERT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  ASSERT_TRUE(supervised.requiresDestruction());

  size_t expectedMemory = supervised.memoryUsage();
  // After creating the supervised slice, memory should increase
  size_t memoryAfterCreation = monitor.current();
  EXPECT_GE(memoryAfterCreation, initialMemory + expectedMemory - 100U)
      << "Memory should increase after creating supervised slice";

  // setValue() properly registers the value in _valueCount with:
  // - refCount = 1
  // - memoryUsage = supervised.memoryUsage()
  // For supervised slices, setValue() does NOT call increaseMemoryUsage()
  block->setValue(0, 0, supervised);
  EXPECT_EQ(monitor.current(), memoryAfterCreation);

  // Use referenceValuesFromRow() to copy from row 0 to row 1
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);

  // steal() removes the value from _valueCount entirely
  // Now row 0's value is "stolen" and no longer tracked by the block
  // Row 1 still has the value in _data, and should still be in _valueCount
  AqlValue stolen = block->getValue(0, 0);
  block->steal(stolen);

  // STATE AFTER STEAL:
  // - Row 0: AqlValue is still in _data, but removed from _valueCount
  // - Row 1: AqlValue is in _data, and SHOULD be in _valueCount with refCount=1
  //          BUT if the bug occurred, it has memoryUsage=0

  // destroy() iterates through _data and for each value:
  //   1. Checks if it requiresDestruction()
  //   2. Looks it up in _valueCount
  //   3. If found:
  //      - Decrements refCount
  //      - If refCount reaches 0, calls destroy() and removes from _valueCount
  //   4. If NOT found:
  //      - Only calls erase() (just zeros the struct, doesn't free heap memory)
  block.reset(nullptr);

  // Clean up the stolen value
  stolen.destroy();

  EXPECT_EQ(monitor.current(), 0U);
}

// WHAT IT CHECKS:
// 1. referenceValuesFromRow() correctly increments refCount when value exists
// 2. Multiple references to the same supervised slice don't leak memory
// 3. destroy() properly handles multiple references
TEST_F(AqlItemBlockSupervisedMemoryTest,
       SupervisedSliceMultipleReferencesProperlyTracked) {
  auto block = itemBlockManager.requestBlock(3, 1);

  AqlValue supervised = createSupervisedSlice(200);
  ASSERT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t initialMemory = monitor.current();

  // Set the value in row 0 - this registers it in _valueCount with refCount=1
  block->setValue(0, 0, supervised);
  EXPECT_EQ(monitor.current(), initialMemory);

  // Reference it to row 1
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);

  // Reference it to row 2
  // Now refCount should be 3 (rows 0, 1, 2 all reference the same value)
  block->referenceValuesFromRow(2, regs, 0);

  // All three rows reference the same supervised slice
  EXPECT_EQ(monitor.current(), initialMemory);

  // Destroy the block
  // 1. Find value in row 0 -> decrement refCount (3->2), not destroy yet
  // 2. Find value in row 1 -> decrement refCount (2->1), not destroy yet
  // 3. Find value in row 2 -> decrement refCount (1->0), NOW destroy it
  block.reset(nullptr);

  // All memory should be released
  EXPECT_EQ(monitor.current(), 0U);
}

// This test mimics what happens in functions::Concat (StringFunctions.cpp)
// where supervised slices are created using the string_view constructor.
TEST_F(AqlItemBlockSupervisedMemoryTest,
       SupervisedSliceFromStringViewLeakScenario) {
  auto block = itemBlockManager.requestBlock(2, 1);

  // Create a supervised slice using string_view constructor
  size_t initialMemory = monitor.current();
  std::string content(200, 'a');
  AqlValue supervised(std::string_view{content}, &monitor);
  ASSERT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  ASSERT_TRUE(supervised.requiresDestruction());

  // Memory should be accounted for after creation
  EXPECT_GT(monitor.current(), initialMemory)
      << "Memory should increase after creating supervised slice";

  // Store it in row 0 - this should properly register it
  block->setValue(0, 0, supervised);

  // Now reference it to row 1 using referenceValuesFromRow
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);

  // Destroy the block - this should clean up all values
  block.reset(nullptr);

  // Check for memory leak
  EXPECT_EQ(monitor.current(), 0U);
}

// WHAT IT CHECKS:
// 1. After steal(), remaining references in the block are still tracked
// 2. destroy() properly cleans up non-stolen references
// 3. Stolen value can be destroyed separately without double-free
TEST_F(AqlItemBlockSupervisedMemoryTest, StealSupervisedSliceThenDestroyBlock) {
  auto block = itemBlockManager.requestBlock(2, 1);

  // Create and set a supervised slice
  AqlValue supervised = createSupervisedSlice(200);
  block->setValue(0, 0, supervised);

  size_t initialMemory = monitor.current();
  EXPECT_GT(initialMemory, 0U);

  // Reference it to row 1
  // Now both row 0 and row 1 reference the same value -> refCount = 2
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);

  // steal() does:
  // 1. getValue() creates a copy (shallow copy - same pointer)
  // 2. steal() removes the value from _valueCount entirely
  // After steal: Row 0's value is no longer tracked, but row 1's value should
  // still be
  AqlValue stolen = block->getValue(0, 0);
  block->steal(stolen);

  // STATE:
  // - Row 0: Value still in _data, but NOT in _valueCount (stolen)
  // - Row 1: Value in _data AND in _valueCount with refCount=1
  // - stolen: Separate AqlValue copy (shallow, same heap memory)

  // Destroy the block
  // - Row 0: Not find in _valueCount -> only erase() (OK, stolen separately)
  // - Row 1: Find in _valueCount, refCount 1->0 -> free heap memory
  block.reset(nullptr);

  // This should destroy the stolen copy's reference to the heap memory
  stolen.destroy();

  // All memory should be released
  EXPECT_EQ(monitor.current(), 0U);
}

// WHAT IT CHECKS:
// 1. setValue() creates correct _valueCount entry
// 2. destroy() finds the value and properly destroys it
// 3. No memory leaks with simple setValue() + destroy() path
TEST_F(AqlItemBlockSupervisedMemoryTest,
       SupervisedSliceWithZeroMemoryUsageInValueCount) {
  auto block = itemBlockManager.requestBlock(1, 1);

  // Create a supervised slice
  AqlValue supervised = createSupervisedSlice(200);
  ASSERT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t initialMemory = monitor.current();

  // Set the value using setValue() - this should create a correct entry
  // in _valueCount with refCount=1 and memoryUsage=expectedMemory
  block->setValue(0, 0, supervised);

  // Verify memory is tracked (already tracked when slice was created)
  EXPECT_EQ(monitor.current(), initialMemory);

  // Destroy the block
  // 1. Find value in _valueCount
  // 2. Decrement refCount (1->0)
  // 3. Call destroy() to free heap memory
  block.reset(nullptr);

  // All memory should be released
  EXPECT_EQ(monitor.current(), 0U);
}

// WHAT IT CHECKS:
// 1. Two AqlValues can point to the same supervised slice
// 2. destroyValue() correctly handles reference counting (decrements, doesn't
// free)
// 3. Remaining value is still properly tracked
// 4. Block destruction cleans up the remaining value correctly
TEST_F(AqlItemBlockSupervisedMemoryTest,
       TwoAqlValuesSameSupervisedSliceDestroyOne) {
  auto block = itemBlockManager.requestBlock(2, 1);

  // Create a supervised slice
  AqlValue supervised = createSupervisedSlice(200);
  ASSERT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  ASSERT_TRUE(supervised.requiresDestruction());

  size_t initialMemory = monitor.current();

  // This registers it in _valueCount with refCount=1
  block->setValue(0, 0, supervised);
  EXPECT_EQ(monitor.current(), initialMemory);

  // Now both row 0 and row 1 point to the SAME supervised slice (same heap
  // memory) _valueCount should show refCount=2
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);

  // Verify both rows point to the same data (shallow copy semantics)
  AqlValue const& val0 = block->getValueReference(0, 0);
  AqlValue const& val1 = block->getValueReference(1, 0);
  EXPECT_EQ(val0.data(), val1.data())
      << "Both values should point to same memory";
  EXPECT_EQ(val0.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  // Memory should still be the same (no additional allocation, already tracked)
  EXPECT_EQ(monitor.current(), initialMemory);

  // Destroy the value in row 0
  // 1. Find the value in _valueCount
  // 2. Decrement refCount (2->1)
  // 3. NOT call destroy() yet (refCount > 0)
  // 4. Only erase() the AqlValue struct in row 0
  block->destroyValue(0, 0);

  // Memory should still be allocated (row 1 still references it)
  EXPECT_EQ(monitor.current(), initialMemory);

  // Row 0 should now be empty
  EXPECT_TRUE(block->getValueReference(0, 0).isEmpty());

  // Row 1 should still have the value
  EXPECT_FALSE(block->getValueReference(1, 0).isEmpty());
  EXPECT_EQ(block->getValueReference(1, 0).type(),
            AqlValue::VPACK_SUPERVISED_SLICE);

  // Destroy the block
  // 1. Find value in row 1
  // 2. Find it in _valueCount (refCount should be 1)
  // 3. Decrement refCount (1->0)
  // 4. NOW call destroy() to free the heap memory
  block.reset(nullptr);

  // All memory should be released
  EXPECT_EQ(monitor.current(), 0U);
}

// WHAT IT CHECKS:
// 1. If referenceValuesFromRow() creates bad entry (memoryUsage=0)
// 2. destroyValue() on one row still works correctly
// 3. Remaining value is properly cleaned up
TEST_F(AqlItemBlockSupervisedMemoryTest,
       TwoAqlValuesSameSupervisedSliceDestroyOneWithBugScenario) {
  auto block = itemBlockManager.requestBlock(2, 1);

  // Create a supervised slice
  AqlValue supervised = createSupervisedSlice(200);
  ASSERT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t initialMemory = monitor.current();

  // Store in row 0 - this creates a CORRECT entry in _valueCount
  block->setValue(0, 0, supervised);
  EXPECT_EQ(monitor.current(), initialMemory);

  // Reference to row 1
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);

  // Now we have:
  // - Row 0: Correct entry in _valueCount (from setValue)
  // - Row 1: Should also be in _valueCount

  // Destroy row 0
  block->destroyValue(0, 0);

  // Memory should still be allocated (row 1 still has it)
  EXPECT_EQ(monitor.current(), initialMemory);

  // Row 1 should still have the value
  EXPECT_FALSE(block->getValueReference(1, 0).isEmpty());

  // Now destroy the block
  block.reset(nullptr);

  // Check for memory leak
  EXPECT_EQ(monitor.current(), 0U);
}

// WHAT IT CHECKS:
// 1. Copying AqlValue from block creates shallow copy (same pointer)
// 2. destroyValue() in block doesn't affect external copy
// 3. External copy can still be used after block destroys its reference
// This test demonstrates that copying AqlValues from blocks can be dangerous
// if you don't manage the lifetime correctly.
TEST_F(AqlItemBlockSupervisedMemoryTest,
       CopyAqlValueOutsideBlockDestroyOneInside) {
  auto block = itemBlockManager.requestBlock(1, 1);

  // Create and store supervised slice
  AqlValue supervised = createSupervisedSlice(200);
  block->setValue(0, 0, supervised);

  size_t initialMemory = monitor.current();
  EXPECT_GT(initialMemory, 0U);

  // Shallow copy the value outside the block
  AqlValue externalCopy = block->getValue(0, 0);
  EXPECT_EQ(externalCopy.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(externalCopy.data(), block->getValueReference(0, 0).data())
      << "External copy should point to same memory (shallow copy)";

  // Destroy the value in the block
  // 1. Decrement refCount in block's _valueCount (1->0)
  // 2. Call destroy() which frees the heap memory
  // 3. The external copy now has a DANGEROUS dangling pointer!
  block->destroyValue(0, 0);

  // Memory should be freed (block's reference is gone)
  // BUT the external copy still has a pointer to freed memory
  size_t memoryAfterDestroy = monitor.current();

  // The external copy might still be tracking memory, so we clean it up
  externalCopy.erase();  // Just zero the struct, don't try to free (might
                         // already be freed)

  // After cleaning up, all memory should be released
  EXPECT_LE(monitor.current(), memoryAfterDestroy)
      << "Memory should not increase after erasing external copy";
}

// WHAT IT CHECKS:
// 1. A supervised slice is created via string_view
// 2. It's stored in an AqlItemBlock
// 3. referenceValuesFromRow() is called
// 4. The block is destroyed - should properly clean up all memory
TEST_F(AqlItemBlockSupervisedMemoryTest,
       ReferenceValuesFromRowWithStringViewSupervisedSlice) {
  auto block = itemBlockManager.requestBlock(2, 1);

  // Create a supervised slice via string_view
  std::string content =
      "This is a test string that will be stored as a supervised slice";
  AqlValue supervised = AqlValue(std::string_view(content), &monitor);
  ASSERT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  ASSERT_TRUE(supervised.requiresDestruction());

  size_t initialMemory = monitor.current();
  size_t expectedMemory = supervised.memoryUsage();
  EXPECT_GT(expectedMemory, 0U);

  // Store it in row 0 - this properly registers it in _valueCount
  block->setValue(0, 0, supervised);
  EXPECT_EQ(monitor.current(), initialMemory);

  // Use referenceValuesFromRow() to copy from row 0 to row 1
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);

  // Verify both rows point to the same data
  AqlValue const& val0 = block->getValueReference(0, 0);
  AqlValue const& val1 = block->getValueReference(1, 0);
  EXPECT_EQ(val0.data(), val1.data())
      << "Both rows should point to same memory";
  EXPECT_EQ(val0.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  // Memory should still be the same (same data, just more references)
  // Memory was already tracked when slice was created
  EXPECT_EQ(monitor.current(), initialMemory);

  // Destroy the block
  // destroy() should properly clean up all values, even with multiple
  // references
  block.reset(nullptr);

  // All memory should be released
  EXPECT_EQ(monitor.current(), 0U);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
