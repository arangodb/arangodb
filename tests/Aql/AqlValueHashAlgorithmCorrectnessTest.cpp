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
static inline AqlValue makeAQLValue(int64_t x) {
  return AqlValue(AqlValueHintInt(x));
}
}  // namespace

class AqlValueHashAlgorithmCorrectnessTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  arangodb::aql::AqlItemBlockManager itemBlockManager{monitor};
  velocypack::Options const* const options{&velocypack::Options::Defaults};
};

// ============================================================================
// toVelocyPack() — deduplication in the raw array
// ============================================================================

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_FormatSpecification_RawArrayStructure) {
  // Positions 0 and 1 of the raw array must be null; actual values start at 2.
  auto block = itemBlockManager.requestBlock(1, 1);
  block->setValue(0, 0, makeAQLValue(42));

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, 1, options, result);
  result.close();

  VPackSlice raw = result.slice().get("raw");
  ASSERT_TRUE(raw.isArray());
  EXPECT_TRUE(raw.at(0).isNull());
  EXPECT_TRUE(raw.at(1).isNull());
  ASSERT_GE(raw.length(), 3U);
  EXPECT_FALSE(raw.at(2).isNull());
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_DeduplicatesSameContent_DifferentPointers) {
  // Two supervised slices with identical content but different pointers must
  // map to the same position in the raw array.
  auto block = itemBlockManager.requestBlock(3, 1);

  std::string content = "shared_content";
  arangodb::velocypack::Builder b1, b2, b3;
  b1.add(arangodb::velocypack::Value(content));
  b2.add(arangodb::velocypack::Value(content));
  b3.add(arangodb::velocypack::Value("different_content"));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));
  AqlValue v3(b3.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b3.slice().byteSize()));

  EXPECT_NE(v1.data(), v2.data()) << "test requires different pointers";

  block->setValue(0, 0, v1);
  block->setValue(1, 0, v2);
  block->setValue(2, 0, v3);

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  VPackSlice raw = result.slice().get("raw");
  ASSERT_TRUE(raw.isArray());
  // v1 and v2 are equal → deduplicated → only 2 unique values (content +
  // different_content)
  EXPECT_EQ(2U, raw.length() - 2);
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_DeduplicatesSameContent_DifferentStorageTypes) {
  // VPACK_INLINE_INT64 (42) and VPACK_INLINE_DOUBLE (42.0) are semantically
  // equal and must deduplicate to the same raw slot.
  auto block = itemBlockManager.requestBlock(3, 1);

  AqlValue v1 = makeAQLValue(42);
  VPackBuilder b;
  b.add(VPackValue(42.0));
  AqlValue v2(b.slice());  // VPACK_INLINE_DOUBLE
  AqlValue v3 = makeAQLValue(100);

  block->setValue(0, 0, v1);
  block->setValue(1, 0, v2);
  block->setValue(2, 0, v3);

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  VPackSlice raw = result.slice().get("raw");
  ASSERT_TRUE(raw.isArray());
  EXPECT_EQ(2U, raw.length() - 2);
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_CrossRowDeduplication) {
  // Same value across different rows and columns must map to the same raw
  // position.
  auto block = itemBlockManager.requestBlock(4, 2);

  AqlValue shared = makeAQLValue(777);
  AqlValue unique1 = makeAQLValue(111);
  AqlValue unique2 = makeAQLValue(222);

  block->setValue(0, 0, shared);
  block->setValue(0, 1, unique1);
  block->setValue(1, 0, shared);
  block->setValue(1, 1, unique2);
  block->setValue(2, 0, shared);
  block->setValue(2, 1, shared);
  block->setValue(3, 0, unique1);
  block->setValue(3, 1, shared);

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, 4, options, result);
  result.close();

  VPackSlice raw = result.slice().get("raw");
  // Only 3 unique values: 777, 111, 222
  EXPECT_EQ(3U, raw.length() - 2);
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_ManyDuplicates_ProvesDeduplication) {
  constexpr size_t numRows = 100;
  auto block = itemBlockManager.requestBlock(numRows, 1);

  std::string content = "duplicated_value";
  for (size_t i = 0; i < numRows; ++i) {
    arangodb::velocypack::Builder b;
    b.add(arangodb::velocypack::Value(content));
    block->setValue(
        i, 0,
        AqlValue(b.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                b.slice().byteSize())));
  }

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, numRows, options, result);
  result.close();

  VPackSlice raw = result.slice().get("raw");
  EXPECT_EQ(1U, raw.length() - 2);
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPack_NegativeZeroAndPositiveZero_Deduplicate) {
  // -0.0 and +0.0 are IEEE-754-equal but have different byte representations.
  // The hash must normalize both to +0.0 so they map to the same raw slot.
  auto block = itemBlockManager.requestBlock(3, 1);

  VPackBuilder bPos, bNeg;
  bPos.add(VPackValue(0.0));
  bNeg.add(VPackValue(-0.0));

  AqlValue vPos(bPos.slice());  // VPACK_INLINE_DOUBLE, +0.0
  AqlValue vNeg(bNeg.slice());  // VPACK_INLINE_DOUBLE, -0.0

  VPackBuilder bOther;
  bOther.add(VPackValue(1.0));
  AqlValue vOther(bOther.slice());

  block->setValue(0, 0, vPos);
  block->setValue(1, 0, vNeg);
  block->setValue(2, 0, vOther);

  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, 3, options, result);
  result.close();

  VPackSlice raw = result.slice().get("raw");
  // +0.0 and -0.0 deduplicate → only 2 unique values (zero + 1.0)
  EXPECT_EQ(2U, raw.length() - 2);
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       toVelocyPackFromVelocyPack_RoundTrip_DeduplicationPreserved) {
  auto sourceBlock = itemBlockManager.requestBlock(4, 2);

  std::string shared = "shared_content_for_roundtrip";
  AqlValue v1(shared);
  AqlValue v2(shared);
  AqlValue v3(std::string("different_content"));

  sourceBlock->setValue(0, 0, v1);
  sourceBlock->setValue(0, 1, v3);
  sourceBlock->setValue(1, 0, v2);
  sourceBlock->setValue(1, 1, v1);
  sourceBlock->setValue(2, 0, v1);
  sourceBlock->setValue(2, 1, v2);
  sourceBlock->setValue(3, 0, v3);
  sourceBlock->setValue(3, 1, v1);

  velocypack::Builder serialized;
  serialized.openObject();
  sourceBlock->toVelocyPack(0, 4, options, serialized);
  serialized.close();

  VPackSlice raw = serialized.slice().get("raw");
  ASSERT_TRUE(raw.isArray());
  EXPECT_EQ(2U, raw.length() - 2);  // shared + different

  auto deserialized = itemBlockManager.requestBlock(4, 2);
  deserialized->initFromSlice(serialized.slice());

  std::equal_to<AqlValue> equal;
  AqlValue const& d00 = deserialized->getValueReference(0, 0);
  AqlValue const& d10 = deserialized->getValueReference(1, 0);
  AqlValue const& d20 = deserialized->getValueReference(2, 0);
  AqlValue const& d30 = deserialized->getValueReference(3, 0);

  EXPECT_TRUE(equal(d00, d10));
  EXPECT_TRUE(equal(d10, d20));
  EXPECT_FALSE(equal(d00, d30));

  // Re-serialization must produce the same deduplication.
  velocypack::Builder reSerialized;
  reSerialized.openObject();
  deserialized->toVelocyPack(0, 4, options, reSerialized);
  reSerialized.close();
  VPackSlice reRaw = reSerialized.slice().get("raw");
  EXPECT_EQ(2U, reRaw.length() - 2);
}

// ============================================================================
// cloneToBlock() — deduplication in the clone cache
// ============================================================================

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_DeduplicatesSameContent_DifferentPointers) {
  auto sourceBlock = itemBlockManager.requestBlock(1, 3);

  std::string shared = "shared_content_for_clone_test";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(shared));
  b2.add(arangodb::velocypack::Value(shared));

  AqlValue v1(b1.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b1.slice().byteSize()));
  AqlValue v2(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                              b2.slice().byteSize()));
  AqlValue v3(std::string("different_content"));

  EXPECT_TRUE(std::equal_to<AqlValue>{}(v1, v2))
      << "test requires equal content";

  sourceBlock->setValue(0, 0, v1);
  sourceBlock->setValue(0, 1, v2);
  sourceBlock->setValue(0, 2, v3);

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2}};
  auto cloned = sourceRow.cloneToBlock(itemBlockManager, registers, 3);
  ASSERT_NE(nullptr, cloned.get());

  std::equal_to<AqlValue> equal;
  EXPECT_TRUE(
      equal(cloned->getValueReference(0, 0), cloned->getValueReference(0, 1)));
  EXPECT_FALSE(
      equal(cloned->getValueReference(0, 0), cloned->getValueReference(0, 2)));
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_ManyDuplicates_ProvesDeduplication) {
  constexpr RegisterId::value_t numCols = 50;
  auto sourceBlock = itemBlockManager.requestBlock(1, numCols);

  std::string content = "duplicated_content";
  for (RegisterId::value_t col = 0; col < numCols; ++col) {
    arangodb::velocypack::Builder b;
    b.add(arangodb::velocypack::Value(content));
    sourceBlock->setValue(
        0, col,
        AqlValue(b.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                b.slice().byteSize())));
  }

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers;
  for (RegisterId::value_t col = 0; col < numCols; ++col) {
    registers.insert(RegisterId{col});
  }
  auto cloned = sourceRow.cloneToBlock(itemBlockManager, registers, numCols);
  ASSERT_NE(nullptr, cloned.get());

  std::equal_to<AqlValue> equal;
  AqlValue const& first = cloned->getValueReference(0, 0);
  for (RegisterId::value_t col = 1; col < numCols; ++col) {
    EXPECT_TRUE(equal(first, cloned->getValueReference(0, col)))
        << "column " << col << " should equal column 0";
  }
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_MemoryEfficiency_ThreeUniqueValues) {
  constexpr RegisterId::value_t numCols = 10;
  auto sourceBlock = itemBlockManager.requestBlock(1, numCols);

  std::string s1 = "shared";
  std::string s2 = "unique_1";
  std::string s3 = "unique_2";

  // Pattern: s1 s2 s1 s3 s1 s1 s1 s2 s1 s3
  std::array<std::string*, 10> pattern = {&s1, &s2, &s1, &s3, &s1,
                                          &s1, &s1, &s2, &s1, &s3};
  for (RegisterId::value_t col = 0; col < numCols; ++col) {
    sourceBlock->setValue(0, col, AqlValue(*pattern[col]));
  }

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers;
  for (RegisterId::value_t col = 0; col < numCols; ++col) {
    registers.insert(RegisterId{col});
  }
  auto cloned = sourceRow.cloneToBlock(itemBlockManager, registers, numCols);
  ASSERT_NE(nullptr, cloned.get());

  std::equal_to<AqlValue> equal;
  AqlValue const& ref_s1 = cloned->getValueReference(0, 0);
  AqlValue const& ref_s2 = cloned->getValueReference(0, 1);
  AqlValue const& ref_s3 = cloned->getValueReference(0, 3);

  for (RegisterId::value_t col : {0u, 2u, 4u, 5u, 6u, 8u}) {
    EXPECT_TRUE(equal(ref_s1, cloned->getValueReference(0, col)));
  }
  for (RegisterId::value_t col : {1u, 7u}) {
    EXPECT_TRUE(equal(ref_s2, cloned->getValueReference(0, col)));
  }
  for (RegisterId::value_t col : {3u, 9u}) {
    EXPECT_TRUE(equal(ref_s3, cloned->getValueReference(0, col)));
  }
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_OnlyClonesDestructibleValues) {
  // Inline values don't go through the clone cache (no destruction needed),
  // but must still be equal after cloning.
  auto sourceBlock = itemBlockManager.requestBlock(1, 3);

  AqlValue inlineInt = makeAQLValue(42);
  AqlValue inlineInt2 = makeAQLValue(42);
  AqlValue managed(std::string("managed_string"));

  sourceBlock->setValue(0, 0, inlineInt);
  sourceBlock->setValue(0, 1, inlineInt2);
  sourceBlock->setValue(0, 2, managed);

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2}};
  auto cloned = sourceRow.cloneToBlock(itemBlockManager, registers, 3);
  ASSERT_NE(nullptr, cloned.get());

  std::equal_to<AqlValue> equal;
  EXPECT_TRUE(
      equal(cloned->getValueReference(0, 0), cloned->getValueReference(0, 1)));
  EXPECT_FALSE(
      equal(cloned->getValueReference(0, 0), cloned->getValueReference(0, 2)));
}

TEST_F(AqlValueHashAlgorithmCorrectnessTest,
       cloneToBlock_EmptyValuesHandledCorrectly) {
  auto sourceBlock = itemBlockManager.requestBlock(1, 3);

  sourceBlock->setValue(0, 0, AqlValue{AqlValueHintNone{}});
  sourceBlock->setValue(0, 1, AqlValue{AqlValueHintNone{}});
  sourceBlock->setValue(0, 2, AqlValue(std::string("content")));

  InputAqlItemRow sourceRow(sourceBlock, 0);
  RegIdFlatSet registers = {RegisterId{0}, RegisterId{1}, RegisterId{2}};
  auto cloned = sourceRow.cloneToBlock(itemBlockManager, registers, 3);
  ASSERT_NE(nullptr, cloned.get());

  EXPECT_TRUE(cloned->getValueReference(0, 0).isEmpty());
  EXPECT_TRUE(cloned->getValueReference(0, 1).isEmpty());
  EXPECT_FALSE(cloned->getValueReference(0, 2).isEmpty());
}
