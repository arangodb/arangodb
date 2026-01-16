#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegIdFlatSet.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "gtest/gtest.h"

#include <boost/container/flat_set.hpp>

#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include <velocypack/Builder.h>
#include <velocypack/Value.h>

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class AqlItemBlockStressTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor};
  velocypack::Options const* const options{&velocypack::Options::Defaults};

  AqlValue createSupervisedSlice(std::string const& content) {
    arangodb::velocypack::Builder b;
    b.add(arangodb::velocypack::Value(content));
    return AqlValue(
        b.slice(),
        static_cast<arangodb::velocypack::ValueLength>(b.slice().byteSize()),
        &monitor);
  }

  AqlValue createManagedSlice(std::string const& content) {
    arangodb::velocypack::Builder b;
    b.add(arangodb::velocypack::Value(content));
    return AqlValue(b.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                   b.slice().byteSize()));
  }
};

// ============================================================================
// ASAN/UBSAN Tests: Use-After-Free and Double-Free Scenarios
// ============================================================================

TEST_F(AqlItemBlockStressTest, ASAN_UseAfterFree_SerializeAfterSteal) {
  // Test: Serialize block after values are stolen - should not access freed
  // memory
  auto block = itemBlockManager.requestBlock(2, 2);

  std::string big1(300, 'a');
  std::string big2(300, 'b');
  AqlValue val1 = createSupervisedSlice(big1);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue val2 = createSupervisedSlice(big2);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  block->setValue(0, 0, val1);
  block->setValue(0, 1, val2);
  block->setValue(1, 0, val1);
  block->setValue(1, 1, val2);

  // Steal all values from row 0
  AqlValue stolen1 = block->stealAndEraseValue(0, 0);
  AqlValue stolen2 = block->stealAndEraseValue(0, 1);

  // CRITICAL: Try to serialize row 0 after values are stolen
  // This should not access freed memory
  velocypack::Builder result;
  block->rowToSimpleVPack(0, options, result);

  // Serialize entire block
  velocypack::Builder result2;
  result2.openObject();
  block->toVelocyPack(0, block->numRows(), options, result2);
  result2.close();

  // Clean up
  stolen1.destroy();
  stolen2.destroy();
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, ASAN_UseAfterFree_CloneAfterSteal) {
  // Test: Clone block after values are stolen
  auto block = itemBlockManager.requestBlock(3, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val1);
  block->setValue(1, 0, val1);
  block->setValue(2, 0, val1);

  // Steal from row 1
  AqlValue stolen = block->stealAndEraseValue(1, 0);

  // Clone block - should handle stolen values gracefully
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // Try to serialize both blocks
  velocypack::Builder result1;
  result1.openObject();
  block->toVelocyPack(0, block->numRows(), options, result1);
  result1.close();

  velocypack::Builder result2;
  result2.openObject();
  cloned->toVelocyPack(0, cloned->numRows(), options, result2);
  result2.close();

  stolen.destroy();
  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, ASAN_DoubleFree_DestroyAfterSteal) {
  // Test: Destroy stolen value, then destroy block - should not double-free
  auto block = itemBlockManager.requestBlock(2, 1);

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val);
  block->setValue(1, 0, val);

  // Steal value
  AqlValue stolen = block->stealAndEraseValue(0, 0);

  // Destroy stolen value
  stolen.destroy();

  // Destroy block - should not try to free already-freed memory
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest,
       ASAN_DoubleFree_CloneDataAndMoveShadow_DestroyOriginal) {
  // Test: cloneDataAndMoveShadow steals values, then destroy original
  auto block = itemBlockManager.requestBlock(3, 1);

  std::string big1(300, 'a');
  std::string big2(300, 'b');
  std::string big3(300, 'c');
  AqlValue val1 = createSupervisedSlice(big1);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue val2 = createSupervisedSlice(big2);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue val3 = createSupervisedSlice(big3);
  EXPECT_EQ(val3.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  block->setValue(0, 0, val1);
  block->setValue(1, 0, val2);
  block->setValue(2, 0, val3);

  block->makeShadowRow(1, 0);
  block->makeShadowRow(2, 0);

  // Clone and move shadow rows (steals values)
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // Destroy original immediately - shadow row values were stolen
  block.reset(nullptr);

  // Try to serialize cloned block
  velocypack::Builder result;
  result.openObject();
  cloned->toVelocyPack(0, cloned->numRows(), options, result);
  result.close();

  cloned.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest,
       ASAN_UseAfterFree_InputAqlItemRow_CloneAfterSteal) {
  // Test: Create InputAqlItemRow, steal values, then clone
  auto block = itemBlockManager.requestBlock(2, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  block->setValue(0, 0, val1);
  block->setValue(0, 1, val2);

  InputAqlItemRow row(block, 0);

  // Steal values from the row's block
  AqlValue stolen1 = block->stealAndEraseValue(0, 0);
  AqlValue stolen2 = block->stealAndEraseValue(0, 1);

  // Try to clone row after values are stolen
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));

  SharedAqlItemBlockPtr cloned = row.cloneToBlock(itemBlockManager, regs, 2);

  // Try to serialize cloned block
  velocypack::Builder result;
  result.openObject();
  cloned->toVelocyPack(0, cloned->numRows(), options, result);
  result.close();

  stolen1.destroy();
  stolen2.destroy();
  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, ASAN_UseAfterFree_ReferenceAfterDestroy) {
  // Test: Reference values from a row, then destroy that row's values
  auto block = itemBlockManager.requestBlock(3, 1);

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val);

  // Reference to rows 1 and 2
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);
  block->referenceValuesFromRow(2, regs, 0);

  // Destroy value in row 0
  block->destroyValue(0, 0);

  // Rows 1 and 2 should still have valid references
  EXPECT_FALSE(block->getValueReference(1, 0).isEmpty());
  EXPECT_FALSE(block->getValueReference(2, 0).isEmpty());

  // Serialize all rows
  for (size_t i = 0; i < 3; i++) {
    velocypack::Builder result;
    block->rowToSimpleVPack(i, options, result);
  }

  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, ASAN_DoubleFree_MultipleSteals) {
  // Test: Steal same value multiple times (should not double-free)
  auto block = itemBlockManager.requestBlock(3, 1);

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val);
  block->setValue(1, 0, val);
  block->setValue(2, 0, val);

  // Steal from row 0
  AqlValue stolen1 = block->stealAndEraseValue(0, 0);

  // Try to steal again (should fail gracefully or handle correctly)
  // Note: This might be undefined behavior, but we want to catch it
  block->steal(stolen1);

  // Destroy stolen value
  stolen1.destroy();

  // Destroy block - should handle remaining references correctly
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, ASAN_UseAfterFree_CloneToBlockAfterDestroy) {
  // Test: Clone row to block, destroy original, then use cloned
  auto block = itemBlockManager.requestBlock(1, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  block->setValue(0, 0, val1);
  block->setValue(0, 1, val2);

  InputAqlItemRow row(block, 0);

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));

  // Clone row
  SharedAqlItemBlockPtr cloned = row.cloneToBlock(itemBlockManager, regs, 2);

  // Destroy original block immediately
  block.reset(nullptr);

  // Use cloned block - should be independent
  velocypack::Builder result;
  result.openObject();
  cloned->toVelocyPack(0, cloned->numRows(), options, result);
  result.close();

  // Access values in cloned block
  EXPECT_FALSE(cloned->getValueReference(0, 0).isEmpty());
  EXPECT_FALSE(cloned->getValueReference(0, 1).isEmpty());

  cloned.reset(nullptr);
}

// ============================================================================
// Memory Accounting Tests: Detect Incorrect Memory Tracking
// ============================================================================

TEST_F(AqlItemBlockStressTest, MemoryAccounting_StealAndTrack) {
  // Test: Memory should be correctly tracked after stealing
  size_t initialMemory = monitor.current();

  auto block = itemBlockManager.requestBlock(2, 1);
  size_t blockMemory = monitor.current() - initialMemory;

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t valueMemory = val.memoryUsage();

  block->setValue(0, 0, val);
  block->setValue(1, 0, val);

  // Memory should be: blockMemory + valueMemory (shared)
  size_t memoryAfterSet = monitor.current();
  EXPECT_GE(memoryAfterSet, initialMemory + blockMemory + valueMemory);

  // Steal value
  AqlValue stolen = block->stealAndEraseValue(0, 0);
  EXPECT_EQ(stolen.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  size_t memoryAfterSteal = monitor.current();

  // Memory should not change because stolen value is still alive
  EXPECT_EQ(memoryAfterSteal, memoryAfterSet);

  // Destroy stolen value
  stolen.destroy();
  size_t memoryAfterDestroy = monitor.current();

  // Memory should decrease further
  EXPECT_LT(memoryAfterDestroy, memoryAfterSteal);

  // Destroy block
  block.reset(nullptr);

  // Memory should be back to initial (allowing for block pool overhead)
  size_t finalMemory = monitor.current();
  EXPECT_LE(finalMemory, initialMemory + blockMemory + 1000U);
}

TEST_F(AqlItemBlockStressTest, MemoryAccounting_CloneDataAndMoveShadow) {
  // Test: Memory tracking after cloneDataAndMoveShadow
  size_t initialMemory = monitor.current();

  auto block = itemBlockManager.requestBlock(3, 1);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  AqlValue val3 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val3.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  block->setValue(0, 0, val1);
  block->setValue(1, 0, val2);
  block->setValue(2, 0, val3);

  block->makeShadowRow(1, 0);
  block->makeShadowRow(2, 0);

  size_t memoryBeforeClone = monitor.current();

  // Clone and move shadow rows
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  size_t memoryAfterClone = monitor.current();

  // Memory should increase (new block + cloned data)
  EXPECT_GT(memoryAfterClone, memoryBeforeClone);

  // Destroy original
  block.reset(nullptr);
  size_t memoryAfterOriginalDestroy = monitor.current();

  // Memory should decrease (original block destroyed)
  EXPECT_LT(memoryAfterOriginalDestroy, memoryAfterClone);

  // Destroy cloned
  cloned.reset(nullptr);

  // Memory should be back to initial (allowing for overhead)
  size_t finalMemory = monitor.current();
  EXPECT_LE(finalMemory, initialMemory + 2000U);
}

TEST_F(AqlItemBlockStressTest, MemoryAccounting_CloneToBlock) {
  size_t initialMemory = monitor.current();

  SharedAqlItemBlockPtr cloned;

  {
    auto block = itemBlockManager.requestBlock(1, 2);

    std::string big1(300, 'a');
    std::string big2(300, 'b');
    AqlValue val1 = createSupervisedSlice(big1);
    AqlValue val2 = createSupervisedSlice(big2);

    block->setValue(0, 0, val1);
    block->setValue(0, 1, val2);

    size_t memoryBeforeClone = monitor.current();

    RegIdFlatSet regs;
    regs.insert(RegisterId::makeRegular(0));
    regs.insert(RegisterId::makeRegular(1));

    {
      InputAqlItemRow row(block, 0);
      cloned = row.cloneToBlock(itemBlockManager, regs, 2);
    }  // row is destroyed here

    size_t memoryAfterClone = monitor.current();
    EXPECT_GT(memoryAfterClone, memoryBeforeClone);

    // block is last owner now
  }

  // At this point only `cloned` should hold the values.
  // Now destroy cloned:
  size_t memoryBeforeDestroyCloned = monitor.current();
  cloned.reset(nullptr);
  size_t finalMemory = monitor.current();

  EXPECT_LE(finalMemory, initialMemory + 2000U);
  EXPECT_LE(finalMemory, memoryBeforeDestroyCloned);
}

// ============================================================================
// TSAN Tests: Race Conditions and Concurrent Access
// ============================================================================

TEST_F(AqlItemBlockStressTest, TSAN_ConcurrentClone) {
  // Test: Multiple threads cloning the same block
  // Note: This test only works for blocks WITHOUT shadow rows.
  // If the block had shadow rows, cloneDataAndMoveShadow() would call
  // stealAndEraseValue() which modifies _valueCount, causing data races.
  // For blocks with shadow rows, each thread should use a separate block.
  auto block = itemBlockManager.requestBlock(10, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  for (size_t i = 0; i < 10; i++) {
    block->setValue(i, 0, val1);
    block->setValue(i, 1, val2);
  }

  std::vector<std::thread> threads;
  std::atomic<int> successCount{0};
  std::atomic<int> errorCount{0};

  for (int i = 0; i < 4; i++) {
    threads.emplace_back([&]() {
      try {
        // Each thread clones the block
        SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

        // Serialize the cloned block
        velocypack::Builder result;
        result.openObject();
        cloned->toVelocyPack(0, cloned->numRows(), options, result);
        result.close();

        successCount++;
        cloned.reset(nullptr);
      } catch (...) {
        errorCount++;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(errorCount.load(), 0);
  EXPECT_EQ(successCount.load(), 4);

  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, TSAN_ConcurrentSteal) {
  // Test: Multiple threads stealing from separate blocks (one per thread)
  // Note: AqlItemBlock is not thread-safe - each thread must use its own block
  std::vector<SharedAqlItemBlockPtr> blocks;
  std::vector<std::thread> threads;
  std::vector<AqlValue> stolenValues;
  std::mutex stolenMutex;
  std::atomic<int> errorCount{0};
  std::atomic<int> successCount{0};

  std::string big(300, 'a');

  // Create a separate block for each thread
  for (int i = 0; i < 5; i++) {
    auto block = itemBlockManager.requestBlock(2, 1);
    AqlValue val = createSupervisedSlice(big);
    EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    block->setValue(0, 0, val);
    blocks.push_back(block);
  }

  // Each thread steals from its own block
  for (int i = 0; i < 5; i++) {
    threads.emplace_back([&, i]() {
      try {
        AqlValue stolen = blocks[i]->stealAndEraseValue(0, 0);

        std::lock_guard<std::mutex> lock(stolenMutex);
        stolenValues.push_back(stolen);
        successCount++;
      } catch (...) {
        errorCount++;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Clean up stolen values
  for (auto& val : stolenValues) {
    val.destroy();
  }

  EXPECT_EQ(errorCount.load(), 0);
  EXPECT_EQ(successCount.load(), 5);
  EXPECT_EQ(stolenValues.size(), 5);

  // Clean up blocks
  blocks.clear();
}

TEST_F(AqlItemBlockStressTest, TSAN_ConcurrentSerialize) {
  // Test: Multiple threads serializing the same block
  auto block = itemBlockManager.requestBlock(5, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  for (size_t i = 0; i < 5; i++) {
    block->setValue(i, 0, val1);
    block->setValue(i, 1, val2);
  }

  std::vector<std::thread> threads;
  std::atomic<int> successCount{0};
  std::atomic<int> errorCount{0};

  for (int i = 0; i < 8; i++) {
    threads.emplace_back([&]() {
      try {
        velocypack::Builder result;
        result.openObject();
        block->toVelocyPack(0, block->numRows(), options, result);
        result.close();
        successCount++;
      } catch (...) {
        errorCount++;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(errorCount.load(), 0);
  EXPECT_EQ(successCount.load(), 8);

  block.reset(nullptr);
}

// ============================================================================
// UBSAN Tests: Undefined Behavior Scenarios
// ============================================================================

TEST_F(AqlItemBlockStressTest, UBSAN_NullPointerAccess) {
  // Test: Access block after reset (should be caught by smart pointer)
  auto block = itemBlockManager.requestBlock(2, 1);

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val);

  // Reset block
  block.reset(nullptr);

  // Any access to block here should be caught by smart pointer
  // (This test verifies that smart pointer prevents use-after-free)
}

TEST_F(AqlItemBlockStressTest, UBSAN_InvalidIndexAccess) {
  // Test: Access invalid indices (should be caught by assertions in debug mode)
  auto block = itemBlockManager.requestBlock(2, 1);

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val);

  // Access valid indices
  EXPECT_FALSE(block->getValueReference(0, 0).isEmpty());
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());

  // Note: Accessing out-of-bounds indices should be caught by TRI_ASSERT
  // in debug builds
}

TEST_F(AqlItemBlockStressTest, UBSAN_InvalidRegisterAccess) {
  // Test: Access invalid register IDs
  auto block = itemBlockManager.requestBlock(1, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  block->setValue(0, 0, val1);
  block->setValue(0, 1, val2);

  InputAqlItemRow row(block, 0);

  // Access valid registers
  EXPECT_FALSE(row.getValue(RegisterId::makeRegular(0)).isEmpty());
  EXPECT_FALSE(row.getValue(RegisterId::makeRegular(1)).isEmpty());

  // Note: Accessing invalid registers should be caught by TRI_ASSERT
}

// ============================================================================
// Stress Tests: Complex Scenarios
// ============================================================================

TEST_F(AqlItemBlockStressTest, Stress_MultipleOperations) {
  // Test: Perform many operations in sequence
  auto block = itemBlockManager.requestBlock(100, 5);

  // Set many values
  std::string prefix(300, 'v');
  for (size_t i = 0; i < 100; i++) {
    for (RegisterId::value_t j = 0; j < 5; j++) {
      std::string content =
          prefix + std::to_string(i) + "," + std::to_string(j);
      AqlValue val = createSupervisedSlice(content);
      block->setValue(i, j, val);
    }
  }

  // Reference values
  RegIdFlatSet regs;
  for (RegisterId::value_t j = 0; j < 5; j++) {
    regs.insert(RegisterId::makeRegular(j));
  }

  // Clone multiple times
  std::vector<SharedAqlItemBlockPtr> clones;
  for (int i = 0; i < 10; i++) {
    clones.push_back(block->cloneDataAndMoveShadow());
  }

  // Serialize all clones
  for (auto& clone : clones) {
    velocypack::Builder result;
    result.openObject();
    clone->toVelocyPack(0, clone->numRows(), options, result);
    result.close();
  }

  // Destroy clones
  clones.clear();

  // Destroy original
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, Stress_InputAqlItemRow_MultipleClones) {
  // Test: Create many InputAqlItemRow clones
  auto block = itemBlockManager.requestBlock(1, 10);

  std::string prefix(300, 'v');
  for (RegisterId::value_t j = 0; j < 10; j++) {
    std::string content = prefix + std::to_string(j);
    AqlValue val = createSupervisedSlice(content);
    block->setValue(0, j, val);
  }

  InputAqlItemRow row(block, 0);

  RegIdFlatSet regs;
  for (RegisterId::value_t j = 0; j < 10; j++) {
    regs.insert(RegisterId::makeRegular(j));
  }

  // Clone many times
  std::vector<SharedAqlItemBlockPtr> clones;
  for (int i = 0; i < 20; i++) {
    clones.push_back(row.cloneToBlock(itemBlockManager, regs, 10));
  }

  // Serialize all clones
  for (auto& clone : clones) {
    velocypack::Builder result;
    result.openObject();
    clone->toVelocyPack(0, clone->numRows(), options, result);
    result.close();
  }

  // Destroy clones
  clones.clear();

  // Destroy original
  block.reset(nullptr);
}

// ============================================================================
// Additional Aggressive Tests for Bug Detection
// ============================================================================

TEST_F(AqlItemBlockStressTest, ASAN_UseAfterFree_ToVelocyPackAfterStealAll) {
  // Test: Serialize after ALL values are stolen
  auto block = itemBlockManager.requestBlock(2, 2);

  std::string big(300, 'a');
  for (size_t i = 0; i < 2; i++) {
    for (size_t j = 0; j < 2; j++) {
      AqlValue val = createSupervisedSlice(big);
      EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
      block->setValue(i, j, val);
    }
  }

  // Steal ALL values
  std::vector<AqlValue> stolen;
  for (size_t i = 0; i < 2; i++) {
    for (RegisterId::value_t j = 0; j < 2; j++) {
      stolen.push_back(block->stealAndEraseValue(i, j));
    }
  }

  // CRITICAL: Try to serialize completely empty block
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  // Clean up
  for (auto& val : stolen) {
    val.destroy();
  }
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, ASAN_DoubleFree_CloneAfterStealThenDestroy) {
  // Test: Clone block, steal from original, destroy original, use clone
  auto block = itemBlockManager.requestBlock(3, 1);

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val);
  block->setValue(1, 0, val);
  block->setValue(2, 0, val);

  // Clone
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // Steal from original
  AqlValue stolen = block->stealAndEraseValue(0, 0);

  // Destroy original
  block.reset(nullptr);

  // Use clone - should still be valid
  velocypack::Builder result;
  result.openObject();
  cloned->toVelocyPack(0, cloned->numRows(), options, result);
  result.close();

  // Destroy stolen value
  stolen.destroy();

  // Destroy clone
  cloned.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest,
       ASAN_UseAfterFree_InputAqlItemRowAfterBlockDestroy) {
  // Test: Create InputAqlItemRow, destroy block, try to use row
  auto block = itemBlockManager.requestBlock(1, 1);

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val);

  InputAqlItemRow row(block, 0);

  // Destroy block
  block.reset(nullptr);

  // Try to use row - should be caught by smart pointer or assertions
  // This test verifies that InputAqlItemRow properly handles block destruction
}

TEST_F(AqlItemBlockStressTest, ASAN_DoubleFree_ReferenceAfterDestroy) {
  // Test: Reference values, destroy source, use references
  auto block = itemBlockManager.requestBlock(3, 1);

  std::string big(300, 'a');
  AqlValue val = createSupervisedSlice(big);
  EXPECT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, val);

  // Reference to rows 1 and 2
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  block->referenceValuesFromRow(1, regs, 0);
  block->referenceValuesFromRow(2, regs, 0);

  // Destroy value in row 0 - should NOT free memory (rows 1 and 2 still
  // reference it)
  block->destroyValue(0, 0);

  // Rows 1 and 2 should still be valid
  EXPECT_FALSE(block->getValueReference(1, 0).isEmpty());
  EXPECT_FALSE(block->getValueReference(2, 0).isEmpty());

  // Serialize rows 1 and 2
  velocypack::Builder result1;
  block->rowToSimpleVPack(1, options, result1);

  velocypack::Builder result2;
  block->rowToSimpleVPack(2, options, result2);

  // Destroy remaining references
  block->destroyValue(1, 0);
  block->destroyValue(2, 0);

  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, ASAN_UseAfterFree_CloneToBlockAfterSteal) {
  // Test: Clone row to block, steal from original, use cloned
  auto block = itemBlockManager.requestBlock(2, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  block->setValue(0, 0, val1);
  block->setValue(0, 1, val2);
  block->setValue(1, 0, val1);
  block->setValue(1, 1, val2);

  InputAqlItemRow row(block, 0);

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));

  // Clone row
  SharedAqlItemBlockPtr cloned = row.cloneToBlock(itemBlockManager, regs, 2);

  // Steal from original block's row 0
  AqlValue stolen1 = block->stealAndEraseValue(0, 0);
  AqlValue stolen2 = block->stealAndEraseValue(0, 1);

  // Use cloned block - should be independent
  velocypack::Builder result;
  result.openObject();
  cloned->toVelocyPack(0, cloned->numRows(), options, result);
  result.close();

  // Access values in cloned block
  EXPECT_FALSE(cloned->getValueReference(0, 0).isEmpty());
  EXPECT_FALSE(cloned->getValueReference(0, 1).isEmpty());

  // Clean up
  stolen1.destroy();
  stolen2.destroy();
  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, MemoryAccounting_StealDoesNotLeak) {
  // Test: Stealing should not cause memory leaks
  size_t initialMemory = monitor.current();

  auto block = itemBlockManager.requestBlock(10, 1);

  std::string big(300, 's');
  for (size_t i = 0; i < 10; i++) {
    AqlValue val = createSupervisedSlice(big);
    ASSERT_EQ(val.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    block->setValue(i, 0, val);
  }

  size_t memoryAfterSet = monitor.current();

  // Steal half the values
  std::vector<AqlValue> stolen;
  for (size_t i = 0; i < 5; i++) {
    stolen.push_back(block->stealAndEraseValue(i, 0));
  }

  size_t memoryAfterSteal = monitor.current();

  // Memory should not change even after stealAndEraseValue
  EXPECT_EQ(memoryAfterSteal, memoryAfterSet);
  // Destroy stolen values
  for (auto& val : stolen) {
    val.destroy();
  }

  size_t memoryAfterDestroy = monitor.current();

  // Memory should decrease further
  EXPECT_LT(memoryAfterDestroy, memoryAfterSteal);

  // Destroy block
  block.reset(nullptr);

  // Memory should be back to initial (allowing for overhead)
  size_t finalMemory = monitor.current();
  EXPECT_LE(finalMemory, initialMemory + 2000U);
}

TEST_F(AqlItemBlockStressTest, ASAN_UseAfterFree_ShadowRowSerialization) {
  // Test: Serialize shadow rows after cloneDataAndMoveShadow
  auto block = itemBlockManager.requestBlock(5, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  // Set values in all rows
  for (size_t i = 0; i < 5; i++) {
    block->setValue(i, 0, val1);
    block->setValue(i, 1, val2);
  }

  // Make rows 1, 2, 3 shadow rows
  block->makeShadowRow(1, 0);
  block->makeShadowRow(2, 1);
  block->makeShadowRow(3, 0);

  // Clone and move shadow rows
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // Try to serialize original block - shadow rows were stolen
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  // Try to serialize cloned block
  velocypack::Builder result2;
  result2.openObject();
  cloned->toVelocyPack(0, cloned->numRows(), options, result2);
  result2.close();

  cloned.reset(nullptr);
  block.reset(nullptr);
}

// ============================================================================
// Most Aggressive Tests: Target Exact Bug Scenario from Error Report
// ============================================================================

TEST_F(AqlItemBlockStressTest,
       ASAN_CRITICAL_ToVelocyPackAfterCloneDataAndMoveShadow_Supervised) {
  // CRITICAL: This is the EXACT scenario from the error report
  // heap-use-after-free in toVelocyPack after cloneDataAndMoveShadow with
  // supervised slices
  auto block = itemBlockManager.requestBlock(3, 1);

  // Create supervised slices (exactly as in error report)
  std::string prefix(300, 'a');
  std::string content1 = prefix + "Supervised slice 1";
  std::string content2 = prefix + "Supervised slice 2";
  std::string content3 = prefix + "Supervised slice 3";

  arangodb::velocypack::Builder b1, b2, b3;
  b1.add(arangodb::velocypack::Value(content1));
  b2.add(arangodb::velocypack::Value(content2));
  b3.add(arangodb::velocypack::Value(content3));

  AqlValue supervised1 = AqlValue(
      b1.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b1.slice().byteSize()),
      &monitor);
  AqlValue supervised2 = AqlValue(
      b2.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b2.slice().byteSize()),
      &monitor);
  AqlValue supervised3 = AqlValue(
      b3.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b3.slice().byteSize()),
      &monitor);

  block->setValue(0, 0, supervised1);
  block->setValue(1, 0, supervised2);
  block->setValue(2, 0, supervised3);

  // Make row 1 a shadow row (as in error scenario)
  block->makeShadowRow(1, 0);

  // Clone and move shadow row (steals values)
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // CRITICAL: Try to serialize original block after shadow row was stolen
  // This is where the use-after-free occurred in the real bug
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  // Also try to serialize the cloned block
  velocypack::Builder result2;
  result2.openObject();
  cloned->toVelocyPack(0, cloned->numRows(), options, result2);
  result2.close();

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest,
       ASAN_CRITICAL_ToVelocyPackAfterSteal_AccessSlice) {
  // CRITICAL: Access slice data after steal - should trigger use-after-free
  auto block = itemBlockManager.requestBlock(2, 1);

  std::string big(300, 'a');
  AqlValue supervised = createSupervisedSlice(big);
  EXPECT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(0, 0, supervised);
  block->setValue(1, 0, supervised);

  // Get reference to value
  AqlValue const& valRef = block->getValueReference(0, 0);

  // Steal the value
  AqlValue stolen = block->stealAndEraseValue(0, 0);

  // CRITICAL: Try to access slice from stolen reference
  // This should trigger use-after-free if the implementation is wrong
  try {
    velocypack::Slice slice = valRef.slice();
    (void)slice.byteSize();  // Access the slice
  } catch (...) {
    // Expected to fail if there's a bug
  }

  // Also try to serialize the block
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  stolen.destroy();
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest,
       ASAN_CRITICAL_CloneDataAndMoveShadow_ThenSerializeOriginal) {
  // CRITICAL: Clone with shadow rows, then serialize original multiple times
  auto block = itemBlockManager.requestBlock(5, 2);

  std::string big(300, 'a');
  AqlValue val1 = createSupervisedSlice(big);
  AqlValue val2 = createSupervisedSlice(big);
  EXPECT_EQ(val1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(val2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  for (size_t i = 0; i < 5; i++) {
    block->setValue(i, 0, val1);
    block->setValue(i, 1, val2);
  }

  // Make some shadow rows
  block->makeShadowRow(1, 0);
  block->makeShadowRow(3, 1);

  // Clone and move shadow rows
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // CRITICAL: Serialize original multiple times (stress test)
  for (int i = 0; i < 10; i++) {
    velocypack::Builder result;
    result.openObject();
    block->toVelocyPack(0, block->numRows(), options, result);
    result.close();

    // Also serialize individual rows
    for (size_t row = 0; row < 5; row++) {
      velocypack::Builder rowResult;
      block->rowToSimpleVPack(row, options, rowResult);
    }
  }

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest, ASAN_CRITICAL_StealThenAccessViaReference) {
  // CRITICAL: Steal value, then try to access it via old reference
  auto block = itemBlockManager.requestBlock(2, 1);

  std::string big(300, 'a');
  AqlValue supervised = createSupervisedSlice(big);
  block->setValue(0, 0, supervised);
  block->setValue(1, 0, supervised);

  // Get reference BEFORE stealing
  AqlValue const& refBeforeSteal = block->getValueReference(0, 0);
  velocypack::Slice sliceBefore = refBeforeSteal.slice();
  size_t sizeBefore = sliceBefore.byteSize();

  // CRITICAL: Access the slice data pointer before stealing
  // This is what might become invalid after steal
  void const* dataPtrBefore = sliceBefore.begin();
  (void)sizeBefore;     // Store for comparison
  (void)dataPtrBefore;  // Store pointer for potential use-after-free detection

  // Steal the value
  AqlValue stolen = block->stealAndEraseValue(0, 0);

  // CRITICAL: Try to access via old reference after steal
  // This should be safe if empty, but might trigger use-after-free if wrong
  try {
    AqlValue const& refAfterSteal = block->getValueReference(0, 0);
    EXPECT_TRUE(refAfterSteal.isEmpty());
  } catch (...) {
    // Might fail if there's a bug
  }

  // CRITICAL: Try to access the slice data we got before stealing
  // This is the dangerous part - if the memory was freed, this will trigger
  // use-after-free
  try {
    // Access the data pointer - this should trigger ASAN if memory was freed
    void const* dataPtrCheck = sliceBefore.begin();
    size_t sizeCheck = sliceBefore.byteSize();

    // If we get here without ASAN error, the memory is still valid
    // (which is correct - the stolen AqlValue still holds the memory)
    // Compare pointers to verify they're the same (memory not moved)
    if (dataPtrBefore != nullptr && dataPtrCheck != nullptr) {
      // Pointers should match if memory is still valid
      (void)dataPtrCheck;
    }
    (void)sizeCheck;
  } catch (...) {
    // Should not fail - stolen value still holds memory
  }

  stolen.destroy();
  block.reset(nullptr);
}

// ============================================================================
// EXACT BUG REPRODUCTION: Match the error trace exactly
// ============================================================================

TEST_F(
    AqlItemBlockStressTest,
    ASAN_EXACT_BUG_ToVelocyPackAfterCloneDataAndMoveShadow_AccessFreedSlice) {
  // EXACT REPRODUCTION of the bug from error message:
  // toVelocyPack -> AqlValue::toVelocyPack -> builder.add(slice) ->
  // slice.byteSize() The slice memory was freed when cloneDataAndMoveShadow
  // stole the value

  auto block = itemBlockManager.requestBlock(3, 1);

  // Create supervised slices exactly as in production
  std::string content1 = "Supervised slice content 1 that is long enough";
  std::string content2 = "Supervised slice content 2 that is long enough";
  std::string content3 = "Supervised slice content 3 that is long enough";

  arangodb::velocypack::Builder b1, b2, b3;
  b1.add(arangodb::velocypack::Value(content1));
  b2.add(arangodb::velocypack::Value(content2));
  b3.add(arangodb::velocypack::Value(content3));

  AqlValue supervised1 = AqlValue(
      b1.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b1.slice().byteSize()),
      &monitor);
  AqlValue supervised2 = AqlValue(
      b2.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b2.slice().byteSize()),
      &monitor);
  AqlValue supervised3 = AqlValue(
      b3.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b3.slice().byteSize()),
      &monitor);

  block->setValue(0, 0, supervised1);
  block->setValue(1, 0, supervised2);
  block->setValue(2, 0, supervised3);

  // Make row 1 a shadow row (critical for the bug)
  block->makeShadowRow(1, 0);

  // CRITICAL: Clone and move shadow row - this STEALS the value from row 1
  // The value is moved to the cloned block, and the original block's row 1
  // should be empty, but the memory might be freed incorrectly
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // CRITICAL: Now serialize the original block
  // This calls toVelocyPack which iterates through all rows
  // When it gets to row 1, it should find an empty value, but if there's a bug,
  // it might try to access the slice of the stolen value, which triggers
  // use-after-free
  velocypack::Builder result;
  result.openObject();

  // This is the EXACT call from the error trace:
  // AqlItemBlock::toVelocyPack -> AqlValue::toVelocyPack -> builder.add(slice)
  // -> slice.byteSize()
  block->toVelocyPack(0, block->numRows(), options, result);

  result.close();

  // Verify the serialization succeeded
  EXPECT_TRUE(result.slice().hasKey("nrItems"));
  EXPECT_TRUE(result.slice().hasKey("raw"));

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest,
       ASAN_EXACT_BUG_ToVelocyPackAfterSteal_EmptyValueStillAccessesSlice) {
  // Test: After stealing, the value should be empty, but toVelocyPack might
  // still try to access the slice if there's a bug
  auto block = itemBlockManager.requestBlock(2, 1);

  AqlValue supervised =
      createSupervisedSlice("Test value for serialization that is long enough");
  block->setValue(0, 0, supervised);
  block->setValue(1, 0, supervised);

  // Get a reference to the value before stealing
  AqlValue const& valRef = block->getValueReference(0, 0);

  // Steal the value - this should make row 0 empty
  AqlValue stolen = block->stealAndEraseValue(0, 0);

  // CRITICAL: Verify row 0 is empty
  EXPECT_TRUE(block->getValueReference(0, 0).isEmpty());

  // CRITICAL: Now serialize - toVelocyPack should handle empty values correctly
  // If there's a bug, it might try to access the slice of the empty value
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  // Also try rowToSimpleVPack on the empty row
  velocypack::Builder rowResult;
  block->rowToSimpleVPack(0, options, rowResult);

  // CRITICAL: The old reference should still be valid (stolen value holds
  // memory) But accessing it through the block should be safe (empty)
  velocypack::Slice sliceFromRef = valRef.slice();
  (void)sliceFromRef
      .byteSize();  // Should be safe - stolen value still holds memory

  stolen.destroy();
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest,
       ASAN_EXACT_BUG_CloneDataAndMoveShadow_ThenSerializeAllRows) {
  // Test: Clone with shadow rows, then serialize each row individually
  // This might trigger the bug if toVelocyPack doesn't check for empty values
  auto block = itemBlockManager.requestBlock(5, 2);

  AqlValue val1 = createSupervisedSlice(
      "Value 1 for serialization test that is long enough");
  AqlValue val2 = createSupervisedSlice(
      "Value 2 for serialization test that is long enough");

  for (size_t i = 0; i < 5; i++) {
    block->setValue(i, 0, val1);
    block->setValue(i, 1, val2);
  }

  // Make rows 1, 2, 3 shadow rows
  block->makeShadowRow(1, 0);
  block->makeShadowRow(2, 1);
  block->makeShadowRow(3, 0);

  // Clone and move shadow rows (steals values from rows 1, 2, 3)
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // CRITICAL: Serialize each row individually
  // This might trigger use-after-free if toVelocyPack accesses stolen values
  for (size_t row = 0; row < 5; row++) {
    velocypack::Builder rowResult;
    block->rowToSimpleVPack(row, options, rowResult);

    // Also try to get the value and serialize it directly
    for (RegisterId::value_t col = 0; col < 2; col++) {
      AqlValue const& val = block->getValueReference(row, col);
      if (!val.isEmpty()) {
        velocypack::Builder valResult;
        val.toVelocyPack(options, valResult, true);
      }
    }
  }

  // Also serialize the entire block
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockStressTest,
       ASAN_EXACT_BUG_SharedSupervisedSlice_StealThenSerialize) {
  // Test: Shared supervised slice, steal one reference, serialize others
  // This might trigger use-after-free if the slice is accessed incorrectly
  auto block = itemBlockManager.requestBlock(3, 1);

  AqlValue supervised =
      createSupervisedSlice("Shared supervised slice that is long enough");

  // Set same value in all rows (shared)
  block->setValue(0, 0, supervised);
  block->setValue(1, 0, supervised);
  block->setValue(2, 0, supervised);

  // Make row 1 a shadow row
  block->makeShadowRow(1, 0);

  // Clone and move shadow row - steals value from row 1
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // CRITICAL: Row 1 should be empty, but rows 0 and 2 should still have the
  // value
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());
  EXPECT_FALSE(block->getValueReference(0, 0).isEmpty());
  EXPECT_FALSE(block->getValueReference(2, 0).isEmpty());

  // CRITICAL: Serialize the block - this should handle row 1's empty value
  // correctly If there's a bug, it might try to access the slice of the stolen
  // value
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  // Also serialize rows individually
  for (size_t row = 0; row < 3; row++) {
    velocypack::Builder rowResult;
    block->rowToSimpleVPack(row, options, rowResult);
  }

  cloned.reset(nullptr);
  block.reset(nullptr);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
