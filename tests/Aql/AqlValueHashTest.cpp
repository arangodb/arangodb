#include "AqlItemBlockHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlValue.h"
#include "Aql/RegIdFlatSet.h"
#include "Basics/GlobalResourceMonitor.h"
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
};

TEST_F(AqlValueHashTest, AqlValueHash_EdgeCase_LargeSupervisedSlices) {
  // Large supervised slices with same content must be equal and hash
  // identically.
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

  EXPECT_TRUE(equal(v1, v2));
  EXPECT_EQ(hasher(v1), hasher(v2));

  v1.destroy();
  v2.destroy();
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlValueHashTest, AqlValueHash_ASAN_PotentialUseAfterFree) {
  // After stealing, look up via a fresh allocation with the same content.
  // Old pointer-based code would miss; content-based must find it.
  auto block = itemBlockManager.requestBlock(2, 1);

  std::string content = "test content for ASAN test";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue v(b.slice(), static_cast<arangodb::velocypack::ValueLength>(
                            b.slice().byteSize()));

  // After setValue the block owns the allocation via _valueCount ref-counting.
  // Do NOT call v.destroy() here — the block will free it; calling destroy()
  // on the original copy would be a double-free of the same raw pointer.
  block->setValue(0, 0, v);

  containers::FlatHashMap<AqlValue, size_t> table;
  table[block->getValueReference(0, 0)] = 100;

  AqlValue stolen = block->stealAndEraseValue(0, 0);
  EXPECT_TRUE(block->getValueReference(0, 0).isEmpty());

  // Different pointer, same content — lookup must be content-based to succeed.
  arangodb::velocypack::Builder b2;
  b2.add(arangodb::velocypack::Value(content));
  AqlValue fresh(b2.slice(), static_cast<arangodb::velocypack::ValueLength>(
                                 b2.slice().byteSize()));

  auto it = table.find(fresh);
  ASSERT_NE(table.end(), it);
  EXPECT_EQ(100, it->second);

  stolen.destroy();
  fresh.destroy();
  block.reset(nullptr);
  EXPECT_EQ(monitor.current(), 0U);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
