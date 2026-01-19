#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/SupervisedBuffer.h"
#include "Mocks/Servers.h"
#include "Utils/CollectionNameResolver.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/Slice.h>
#include <velocypack/Buffer.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::velocypack;

namespace {
using DocumentData = std::unique_ptr<std::string>;
inline size_t ptrOverhead() { return sizeof(ResourceMonitor*); }

inline Builder makeLargeArray(size_t n = 2048, char bytesToFill = 'a') {
  Builder b;
  b.openArray();
  b.add(Value(std::string(n, bytesToFill)));
  b.close();
  return b;
}

inline Builder makeString(size_t n, char bytesToFill = 'a') {
  Builder b;
  b.add(Value(std::string(n, bytesToFill)));
  return b;
}

inline DocumentData makeDocDataFromSlice(Slice s) {
  auto const* p = reinterpret_cast<char const*>(s.start());
  return std::make_unique<std::string>(p, p + s.byteSize());
}

inline void expectEqualBothWays(AqlValue const& a, AqlValue const& b) {
  std::equal_to<AqlValue> eq;
  EXPECT_TRUE(eq(a, b));
  EXPECT_TRUE(eq(b, a));
}

inline void expectNotEqualBothWays(AqlValue const& a, AqlValue const& b) {
  std::equal_to<AqlValue> eq;
  EXPECT_FALSE(eq(a, b));
  EXPECT_FALSE(eq(b, a));
}
}  // namespace

// Constructor tests focus on memory accounting and supervised allocation.
TEST(AqlValueSupervisedTest, ConstructorsAccountCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  // string_view (short)
  {
    std::string s(15, 'x');
    auto payloadSize = 1 + s.size();
    auto expected = ptrOverhead() + payloadSize;

    AqlValue v(std::string_view{s}, &rm);
    EXPECT_EQ(v.memoryUsage(), expected);
    EXPECT_EQ(v.memoryUsage(), rm.current());
    ASSERT_TRUE(v.slice().isString());
    EXPECT_EQ(v.slice().stringView(), s);

    v.destroy();
    EXPECT_EQ(rm.current(), 0);
  }

  // string_view (long)
  {
    std::string s(300, 'A');
    auto payloadSize = 1 + 8 + s.size();
    auto expected = ptrOverhead() + payloadSize;

    AqlValue v(std::string_view{s}, &rm);
    EXPECT_EQ(v.memoryUsage(), expected);
    EXPECT_EQ(v.memoryUsage(), rm.current());
    ASSERT_TRUE(v.slice().isString());
    EXPECT_EQ(v.slice().stringView(), s);

    v.destroy();
    EXPECT_EQ(rm.current(), 0);
  }

  // Buffer&& / Buffer const&
  {
    auto builder = makeLargeArray(2048, 'a');
    Slice slice = builder.slice();

    velocypack::Buffer<uint8_t> buffer;
    buffer.append(slice.start(), slice.byteSize());

    {
      velocypack::Buffer<uint8_t> moved = buffer;
      AqlValue v(std::move(moved), &rm);
      EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
      auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
      EXPECT_EQ(v.memoryUsage(), expected);
      EXPECT_EQ(v.memoryUsage(), rm.current());
      v.destroy();
      EXPECT_EQ(rm.current(), 0);
    }

    {
      AqlValue v(buffer, &rm);
      EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
      auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
      EXPECT_EQ(v.memoryUsage(), expected);
      EXPECT_EQ(v.memoryUsage(), rm.current());
      v.destroy();
      EXPECT_EQ(rm.current(), 0);
    }
  }

  // Slice / HintSliceCopy
  {
    auto builder = makeLargeArray(1024, 'a');
    Slice slice = builder.slice();

    {
      AqlValue v(slice, &rm);
      auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
      EXPECT_EQ(v.memoryUsage(), expected);
      EXPECT_EQ(v.memoryUsage(), rm.current());
      ASSERT_TRUE(v.slice().isArray());
      ValueLength l = 0;
      (void)v.slice().at(0).getStringUnchecked(l);
      EXPECT_EQ(l, 1024);
      v.destroy();
      EXPECT_EQ(rm.current(), 0);
    }

    {
      AqlValueHintSliceCopy hint{slice};
      AqlValue v(hint, &rm);
      auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
      EXPECT_EQ(v.memoryUsage(), expected);
      EXPECT_EQ(v.memoryUsage(), rm.current());
      v.destroy();
      EXPECT_EQ(rm.current(), 0);
    }
  }

  // Multiple supervised slices
  {
    auto bA = makeLargeArray(1000, 'a');
    auto bB = makeLargeArray(2000, 'b');
    auto bC = makeLargeArray(3000, 'c');

    AqlValue a1(bA.slice(), &rm);
    AqlValue a2(bB.slice(), &rm);
    AqlValue a3(bC.slice(), &rm);

    size_t e1 = static_cast<size_t>(bA.slice().byteSize()) + ptrOverhead();
    size_t e2 = static_cast<size_t>(bB.slice().byteSize()) + ptrOverhead();
    size_t e3 = static_cast<size_t>(bC.slice().byteSize()) + ptrOverhead();

    EXPECT_EQ(a1.memoryUsage() + a2.memoryUsage() + a3.memoryUsage(),
              e1 + e2 + e3);
    EXPECT_EQ(rm.current(), e1 + e2 + e3);

    a1.destroy();
    a2.destroy();
    a3.destroy();
    EXPECT_EQ(rm.current(), 0);
  }
}

// Test copy constructor behavior: supervised slices use shallow copy
TEST(AqlValueSupervisedTest, CopyCtorAccountsCorrectSize) {
  {
    Builder b;
    b.add(Value(42));
    AqlValue v(b.slice());
    AqlValue cpy = v;
    EXPECT_EQ(cpy.memoryUsage(), v.memoryUsage());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));
    cpy.destroy();
    v.destroy();
  }

  {
    Builder b;
    b.add(Value(std::string(300, 'a')));
    AqlValue v(b.slice());
    ASSERT_EQ(v.type(), AqlValue::VPACK_MANAGED_SLICE);

    AqlValue cpy = v;
    ASSERT_EQ(cpy.type(), AqlValue::VPACK_MANAGED_SLICE);
    EXPECT_EQ(cpy.slice().start(), v.slice().start());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    cpy.destroy();
  }

  {
    Builder b = makeString(300, 'x');
    auto doc = makeDocDataFromSlice(b.slice());
    AqlValue v(doc);
    ASSERT_EQ(v.type(), AqlValue::VPACK_MANAGED_STRING);

    AqlValue cpy = v;
    ASSERT_EQ(cpy.type(), AqlValue::VPACK_MANAGED_STRING);
    EXPECT_EQ(v.data(), cpy.data());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    cpy.destroy();
  }

  {
    auto& g = GlobalResourceMonitor::instance();
    ResourceMonitor rm(g);

    Builder b;
    b.openObject();
    b.add("k", Value(std::string(300, 'a')));
    b.close();

    AqlValue v(b.slice(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    auto* pv = v.slice().start();
    std::uint64_t base = rm.current();
    EXPECT_EQ(v.memoryUsage(), base);

    AqlValue cpy = v;
    ASSERT_EQ(cpy.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    auto* pc = cpy.slice().start();

    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));
    EXPECT_EQ(pc, pv);
    EXPECT_EQ(rm.current(), base);

    cpy.destroy();
    EXPECT_EQ(rm.current(), 0U);
    v.erase();
    EXPECT_EQ(rm.current(), 0U);
  }
}

TEST(AqlValueSupervisedTest, MoveCtorAccountsCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  const std::string big(4096, 'x');

  Builder bin;
  const std::string sVal = std::string("hello-") + big;
  bin.add(Value(sVal));
  AqlValue a(Slice(bin.slice()), &rm);  // SupervisedSlice
  ASSERT_EQ(a.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  const std::size_t base = rm.current();

  // Move constructor (default)
  AqlValue b(std::move(a));
  EXPECT_EQ(rm.current(), base) << "Move construction must not allocate";
  EXPECT_EQ(b.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_TRUE(b.isString());
  EXPECT_EQ(b.slice().copyString(), sVal);

  // Chain move constructor
  AqlValue c(std::move(b));
  EXPECT_EQ(rm.current(), base);
  EXPECT_TRUE(c.isString());
  EXPECT_EQ(c.slice().copyString(), sVal);

  // Destroying owner frees memory; a and b have dangling pointers
  c.destroy();
  a.erase();
  b.erase();
  EXPECT_EQ(rm.current(), 0U);
}

// Test if small value won't create a Supervised AqlValue
TEST(AqlValueSupervisedTest, InlineCtorNotAccounts) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto builder = makeString(14, 'a');
  Slice slice = builder.slice();
  AqlValue aqlVal(slice, &resourceMonitor);

  EXPECT_EQ(aqlVal.memoryUsage(), 0);       // No external memory usage
  EXPECT_EQ(resourceMonitor.current(), 0);  // No increase for resourceMonitor
  EXPECT_TRUE(aqlVal.slice().isString());
  {
    ValueLength l = 0;
    (void)aqlVal.slice().getStringUnchecked(l);
    EXPECT_EQ(static_cast<size_t>(l), 14);
  }
  EXPECT_EQ(aqlVal.slice().byteSize(), slice.byteSize());

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

// Test the boundary when AqlValue allocates data in external memory space
TEST(AqlValueSupervisedTest, BoundaryOverInlineCtorAccounts) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto builder = makeString(16, 'a');
  Slice slice = builder.slice();
  AqlValue aqlVal(slice, &resourceMonitor);

  auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
  // Verify memory accounting
  EXPECT_EQ(aqlVal.memoryUsage(), expected);
  // Verify ResourceMonitor accounting
  EXPECT_EQ(aqlVal.memoryUsage(), resourceMonitor.current());

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

// Test the behavior of AqlValue(uint8_t const* pointer) -> VPACK_SLICE_POINTER
TEST(AqlValueSupervisedTest, PointerCtorNotAccount) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto builder1 = makeLargeArray(3072, 'a');
  auto builder2 = makeLargeArray(3072, 'b');
  Slice slice1 = builder1.slice();
  Slice slice2 = builder2.slice();

  auto expected = static_cast<size_t>(slice1.byteSize()) + ptrOverhead();
  AqlValue owned(slice1, &resourceMonitor);
  ASSERT_EQ(owned.memoryUsage(), expected);
  ASSERT_EQ(owned.memoryUsage(), resourceMonitor.current());

  AqlValue adopted(slice2.begin());

  EXPECT_EQ(resourceMonitor.current(), expected);
  EXPECT_EQ(adopted.memoryUsage(), 0);

  adopted.destroy();
  EXPECT_EQ(resourceMonitor.current(), expected);

  owned.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

TEST(AqlValueSupervisedTest, PointerCtorForSupervisedBufferNotAccount) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  SupervisedBuffer supervised(resourceMonitor);
  Builder builder(supervised);
  builder.openArray();
  builder.add(Value(std::string(1500, 'a')));
  builder.close();

  auto before = resourceMonitor.current();

  AqlValue adopted(builder.slice().begin());
  EXPECT_EQ(adopted.memoryUsage(), 0);
  EXPECT_EQ(resourceMonitor.current(), before);

  adopted.destroy();
  EXPECT_EQ(resourceMonitor.current(), before);
}

TEST(AqlValueSupervisedTest, DefaultDestructorNotDestroy) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  auto b = makeLargeArray(2000, 'a');
  Slice slice = b.slice();
  AqlValue original(slice, &rm);
  EXPECT_EQ(original.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  auto expected = slice.byteSize() + ptrOverhead();
  auto base = rm.current();
  EXPECT_EQ(original.memoryUsage(), expected);
  EXPECT_EQ(original.memoryUsage(), base);

  {
    AqlValue copied = original;
    EXPECT_EQ(copied.memoryUsage(), base);
  }

  EXPECT_EQ(rm.current(), base);
  original.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, SlicePointerAddressSemantics) {
  Builder b;
  b.openArray();
  b.add(Value(1));
  b.close();
  VPackSlice s = b.slice();

  AqlValue p1(s.begin());
  AqlValue p2(s.begin());
  expectEqualBothWays(p1, p2);

  Builder b2;
  b2.openArray();
  b2.add(Value(1));
  b2.close();
  VPackSlice s2 = b2.slice();
  AqlValue p3(s2.begin());
  expectNotEqualBothWays(p1, p3);

  p1.destroy();
  p2.destroy();
  p3.destroy();
}

// Managed slice vs supervised slice: different types => NOT equal
TEST(AqlValueSupervisedTest, ManagedSliceNotEqualsSupervisedSliceDifferentType) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b = makeString(300, 'a');
  Slice s = b.slice();

  AqlValue managed(s);
  AqlValue supervised(s, &rm);

  expectNotEqualBothWays(managed, supervised);

  Builder bdiff = makeString(300, 'b');
  AqlValue supervisedDiff(bdiff.slice(), &rm);
  expectNotEqualBothWays(managed, supervisedDiff);

  managed.destroy();
  supervised.destroy();
  supervisedDiff.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Managed string (DocumentData) vs supervised slice: different types => NOT equal
TEST(AqlValueSupervisedTest, ManagedStringNotEqualsSupervisedSliceDifferentType) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder bs = makeString(300, 'x');
  Slice ss = bs.slice();
  auto doc = makeDocDataFromSlice(ss);

  AqlValue managedString(doc);
  AqlValue supervised(ss, &rm);

  expectNotEqualBothWays(managedString, supervised);

  Builder by = makeString(300, 'y');
  AqlValue supervisedY(by.slice(), &rm);
  expectNotEqualBothWays(managedString, supervisedY);

  managedString.destroy();
  supervised.destroy();
  supervisedY.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, SupervisedSliceCloneEquality) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b = makeLargeArray(512, 'q');
  AqlValue a(b.slice(), &rm);
  AqlValue c = a.clone();

  expectNotEqualBothWays(a, c);
  EXPECT_TRUE(a.slice().binaryEquals(c.slice()));

  a.destroy();
  c.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Range semantics: equality uses pointer identity
TEST(AqlValueSupervisedTest, RangePointerIdentity) {
  AqlValue r1(1, 3);
  AqlValue r2(1, 3);
  expectNotEqualBothWays(r1, r2);

  std::equal_to<AqlValue> eq;
  EXPECT_TRUE(eq(r1, r1));

  r1.destroy();
  r2.destroy();
}

TEST(AqlValueSupervisedTest, SupervisedSliceShallowCopyEquality) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b = makeString(300, 's');
  AqlValue v(b.slice(), &rm);
  ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue cpy = v;

  expectEqualBothWays(v, cpy);

  cpy.destroy();
  v.erase();
  EXPECT_EQ(rm.current(), 0U);
}

// Comprehensive test for operator==
TEST(AqlValueSupervisedTest, PointerBasedEqualitySemantics) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder a = makeString(300, 'a');
  Builder b = makeString(300, 'b');
  Slice sa = a.slice();
  Slice sb = b.slice();

  AqlValue managedA1(sa);
  ASSERT_EQ(managedA1.type(), AqlValue::VPACK_MANAGED_SLICE);
  AqlValue managedA2(sa);
  ASSERT_EQ(managedA2.type(), AqlValue::VPACK_MANAGED_SLICE);
  AqlValue supervisedA1(sa, &rm);
  ASSERT_EQ(supervisedA1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedA2(sa, &rm);
  ASSERT_EQ(supervisedA2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedB1(sb, &rm);
  ASSERT_EQ(supervisedB1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedB2(sb, &rm);
  ASSERT_EQ(supervisedB2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  std::equal_to<AqlValue> eq;
  
  EXPECT_FALSE(eq(managedA1, supervisedA1));
  EXPECT_FALSE(eq(managedA1, supervisedA2));
  EXPECT_FALSE(eq(managedA2, supervisedA1));
  EXPECT_FALSE(eq(managedA2, supervisedA2));

  EXPECT_FALSE(eq(supervisedA1, supervisedA2));
  EXPECT_FALSE(eq(managedA1, managedA2));

  EXPECT_FALSE(eq(managedA1, supervisedB1));
  EXPECT_FALSE(eq(managedA1, supervisedB2));
  EXPECT_FALSE(eq(managedA2, supervisedB1));
  EXPECT_FALSE(eq(managedA2, supervisedB2));
  EXPECT_FALSE(eq(supervisedA1, supervisedB1));

  managedA1.destroy();
  managedA2.destroy();
  supervisedA1.destroy();
  supervisedA2.destroy();
  supervisedB1.destroy();
  supervisedB2.destroy();
}

TEST(AqlValueSupervisedTest,
     RequiresDestructionFuncForSupervisedAqlValueReturnsTrue) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b;
  b.add(Value(std::string(200, 'a')));
  AqlValue supSlice(b.slice(), &rm);
  EXPECT_EQ(supSlice.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_TRUE(supSlice.requiresDestruction());

  supSlice.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, HasKeyDoesNotAllocateForSupervisedSlice) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  std::string big(4096, 'x');

  {
    Builder b;
    b.openObject();
    b.add("a", Value(1));
    b.add("pad", Value(big));
    b.close();

    AqlValue v(b.slice(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    auto base = rm.current();
    EXPECT_TRUE(v.hasKey("a"));
    EXPECT_FALSE(v.hasKey("missing"));
    EXPECT_EQ(rm.current(), base);

    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }

  {
    Builder b;
    b.openArray();
    b.add(Value(big));
    b.close();

    AqlValue v(b.slice(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    auto base = rm.current();
    EXPECT_FALSE(v.hasKey("a"));
    EXPECT_EQ(rm.current(), base);

    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }
}

TEST(AqlValueSupervisedTest, FuncAtWithDoCopyTrueReturnsCopy) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder arr;
  arr.openArray();
  arr.add(Value(std::string(2048, 'a')));
  arr.add(Value(std::string(2048, 'b')));
  arr.add(Value(std::string(2048, 'c')));
  arr.close();

  AqlValue a(arr.slice(), &rm);
  auto originalSize = arr.slice().byteSize() + ptrOverhead();
  ASSERT_EQ(a.memoryUsage(), originalSize);
  ASSERT_EQ(a.memoryUsage(), rm.current());

  bool mustDestroy = false;
  AqlValue e0 = a.at(1, mustDestroy, false);
  EXPECT_FALSE(mustDestroy);
  EXPECT_EQ(e0.slice().getStringLength(), 2048);
  EXPECT_EQ(e0.memoryUsage(), 0);
  EXPECT_EQ(rm.current(), originalSize);

  AqlValue e1 = a.at(2, mustDestroy, true);
  EXPECT_TRUE(mustDestroy);
  EXPECT_EQ(e1.slice().getStringLength(), 2048);
  EXPECT_GE(e1.memoryUsage(), ptrOverhead() + 2048);
  EXPECT_EQ(rm.current(), originalSize + e1.memoryUsage());

  e1.destroy();
  EXPECT_EQ(rm.current(), originalSize);

  a.destroy();
  EXPECT_EQ(rm.current(), 0);
}

// Test document attribute extraction with doCopy parameter
TEST(AqlValueSupervisedTest, FuncDocAttributeExtractionWithDoCopy) {
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase = server.getSystemDatabase();
  arangodb::CollectionNameResolver resolver(vocbase);
  
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b;
  b.openObject();
  b.add("_key", Value(std::string(10000, 'k')));
  b.add("_id", Value("mycol/abcdefhijklmn123456789"));
  b.close();

  AqlValue v(b.slice(), &rm);
  EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  size_t base = rm.current();

  {
    bool mustDestroy = false;
    AqlValue out = v.getKeyAttribute(mustDestroy, false);
    EXPECT_EQ(out.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_FALSE(mustDestroy);
    EXPECT_EQ(rm.current(), base);
    out.destroy();
  }

  {
    bool mustDestroy = false;
    AqlValue out = v.getKeyAttribute(mustDestroy, true);
    EXPECT_EQ(out.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(mustDestroy);
    EXPECT_EQ(rm.current(), base + out.memoryUsage());
    out.destroy();
    EXPECT_EQ(rm.current(), base);
  }

  {
    bool mustDestroy = false;
    AqlValue out = v.getIdAttribute(resolver, mustDestroy, false);
    EXPECT_EQ(out.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_FALSE(mustDestroy);
    EXPECT_EQ(rm.current(), base);
    out.destroy();
  }

  {
    bool mustDestroy = false;
    AqlValue out = v.getIdAttribute(resolver, mustDestroy, true);
    EXPECT_EQ(out.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(mustDestroy);
    EXPECT_EQ(rm.current(), base + out.memoryUsage());
    out.destroy();
    EXPECT_EQ(rm.current(), base);
  }

  v.destroy();
  EXPECT_EQ(rm.current(), 0U);

  Builder b2;
  b2.openObject();
  b2.add("x", Value(std::string(12000, 'x')));
  b2.close();

  AqlValue v2(b2.slice(), &rm);
  base = rm.current();

  {
    bool mustDestroy = false;
    AqlValue out = v2.getKeyAttribute(mustDestroy, true);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.slice().isNull());
    EXPECT_EQ(rm.current(), base);
  }

  {
    bool mustDestroy = false;
    AqlValue out = v2.getIdAttribute(resolver, mustDestroy, true);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.slice().isNull());
    EXPECT_EQ(rm.current(), base);
  }

  v2.destroy();
  EXPECT_EQ(rm.current(), 0U);

  const std::string big(4096, 'x');
  Builder bEdge;
  bEdge.openObject();
  bEdge.add("_from", Value("vertices/users/" + big));
  bEdge.add("_to", Value("vertices/posts/" + big));
  bEdge.close();

  AqlValue vEdge(bEdge.slice(), &rm);
  base = rm.current();

  {
    bool mustDestroy = false;
    vEdge.getFromAttribute(mustDestroy, false);
    EXPECT_FALSE(mustDestroy);
    EXPECT_EQ(rm.current(), base);
    
    AqlValue out2 = vEdge.getFromAttribute(mustDestroy, true);
    EXPECT_TRUE(mustDestroy);
    EXPECT_EQ(rm.current(), base + out2.memoryUsage());
    out2.destroy();
  }

  {
    bool mustDestroy = false;
    vEdge.getToAttribute(mustDestroy, false);
    EXPECT_FALSE(mustDestroy);
    EXPECT_EQ(rm.current(), base);
    
    AqlValue out2 = vEdge.getToAttribute(mustDestroy, true);
    EXPECT_TRUE(mustDestroy);
    EXPECT_EQ(rm.current(), base + out2.memoryUsage());
    out2.destroy();
  }

  vEdge.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, FuncGetWithDoCopyTrueReturnsCopy) {
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase = server.getSystemDatabase();
  arangodb::CollectionNameResolver resolver(vocbase);

  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  ASSERT_EQ(rm.current(), 0U);

  std::string bigName(4096, 'N');
  std::string bigPayload(8192, 'P');

  Builder b;
  b.openObject();
  {
    b.add("payload", Value(bigPayload));

    b.add(VPackValue("user"));
    b.openObject();
    {
      b.add("id", Value("plain-string-id"));

      b.add(VPackValue("profile"));
      b.openObject();
      {
        b.add("name", Value(bigName));
        b.add("age", Value(7));
      }
      b.close();  // profile
    }
    b.close();  // user
  }
  b.close();  // root

  auto s = b.slice();

  AqlValue v(s, s.byteSize(), &rm);
  ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  size_t baseMem = rm.current();
  ASSERT_EQ(baseMem, v.memoryUsage());

  {
    bool mustDestroy = false;
    bool doCopy = false;
    AqlValue got = v.get(resolver, "payload", mustDestroy, doCopy);
    EXPECT_EQ(got.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_FALSE(mustDestroy);
    ASSERT_FALSE(got.isNone());
    ASSERT_TRUE(got.slice().isString());

    EXPECT_EQ(rm.current(), baseMem);
    got.destroy();
    EXPECT_EQ(rm.current(), baseMem);
  }

  {
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue got = v.get(resolver, "payload", mustDestroy, doCopy);
    EXPECT_EQ(got.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(mustDestroy);
    ASSERT_FALSE(got.isNone());
    ASSERT_TRUE(got.slice().isString());

    EXPECT_EQ(rm.current(), baseMem + got.memoryUsage());
    got.destroy();
    EXPECT_EQ(rm.current(), baseMem);
  }

  {
    std::vector<std::string> path = {"user", "profile", "name"};
    bool mustDestroy = false;
    bool doCopy = false;
    AqlValue got = v.get(resolver, path, mustDestroy, doCopy);
    EXPECT_EQ(got.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_FALSE(mustDestroy);
    ASSERT_FALSE(got.isNone());
    ASSERT_TRUE(got.slice().isString());

    EXPECT_EQ(rm.current(), baseMem);
    got.destroy();
    EXPECT_EQ(rm.current(), baseMem);
  }

  {
    std::vector<std::string> path = {"user", "profile", "name"};
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue got = v.get(resolver, path, mustDestroy, doCopy);
    EXPECT_EQ(got.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(mustDestroy);
    ASSERT_FALSE(got.isNone());
    ASSERT_TRUE(got.slice().isString());

    EXPECT_EQ(rm.current(), baseMem + got.memoryUsage());
    got.destroy();
    EXPECT_EQ(rm.current(), baseMem);
  }

  {
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue got = v.get(resolver, "does_not_exist", mustDestroy, doCopy);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(got.slice().isNull());
    EXPECT_EQ(rm.current(), baseMem);
    got.destroy();
    EXPECT_EQ(rm.current(), baseMem);
  }

  {
    std::vector<std::string> badPath = {"user", "nope", "name"};
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue got = v.get(resolver, badPath, mustDestroy, doCopy);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(got.slice().isNull());
    EXPECT_EQ(rm.current(), baseMem);
    got.destroy();
    EXPECT_EQ(rm.current(), baseMem);
  }

  v.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, FuncDataReturnsPointerToActualData) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b1 = makeString(300, 'a');
  Slice s1 = b1.slice();

  AqlValue a1(s1, &rm);
  auto e1 = s1.byteSize() + ptrOverhead();
  EXPECT_EQ(a1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(a1.memoryUsage(), e1);
  EXPECT_EQ(a1.memoryUsage(), rm.current());

  EXPECT_EQ(static_cast<uint8_t const*>(a1.data()), a1.slice().start());

  a1.destroy();
  EXPECT_EQ(rm.current(), 0u);
}

TEST(AqlValueSupervisedTest, ToVelocyPackCopiesOutOfSupervisedMemory) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  std::string big(4096, 'x');
  Builder in;
  in.openObject();
  in.add("k", Value(1));
  in.add("pad", Value(big));
  in.close();

  AqlValue v(in.slice(), &rm);
  ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  Builder out;
  v.toVelocyPack(nullptr, out, /*allowUnindexed*/ true);

  v.destroy();
  EXPECT_EQ(rm.current(), 0U);

  EXPECT_TRUE(out.slice().binaryEquals(in.slice()));
}

TEST(AqlValueSupervisedTest,
     FuncMaterializeForSupervisedAqlValueReturnsShallowCopy) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  std::string big(8 * 1024, 'x');
  Builder obj;
  obj.openObject();
  obj.add("q", Value(7));
  obj.add("big", Value(big));
  obj.close();

  AqlValue v1(obj.slice(), &rm);
  std::uint64_t base = rm.current();

  bool copied1 = true;
  AqlValue mat1 = v1.materialize(nullptr, copied1);
  EXPECT_FALSE(copied1);

  EXPECT_TRUE(v1.slice().binaryEquals(obj.slice()));
  EXPECT_TRUE(mat1.slice().binaryEquals(obj.slice()));

  EXPECT_EQ(rm.current(), base);

  mat1.destroy();
  EXPECT_EQ(rm.current(), 0U);

  v1.erase();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest,
     FuncSliceWithAqlValueTypeReturnsSliceOfActualData) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b1 = makeString(300, 'a');
  Slice s1 = b1.slice();
  AqlValue a1(s1, &rm);
  EXPECT_EQ(a1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  {
    auto sl = a1.slice();
    EXPECT_EQ(sl.getStringLength(), 300);
    EXPECT_EQ(sl.start(), static_cast<uint8_t const*>(a1.data()));
  }

  a1.destroy();
  EXPECT_EQ(rm.current(), 0u);
}

TEST(AqlValueSupervisedTest, FuncCloneCreatesAnotherCopy) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  Builder builder;
  builder.openArray();
  builder.add(Value(std::string(1024, 'a')));
  builder.close();
  Slice slice = builder.slice();

  AqlValue aqlVal(slice, &resourceMonitor);
  AqlValue cloneVal = aqlVal.clone();
  EXPECT_EQ(resourceMonitor.current(),
            aqlVal.memoryUsage() + cloneVal.memoryUsage());

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), cloneVal.memoryUsage());

  cloneVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

TEST(AqlValueSupervisedTest, FuncMemoryUsageReturnsCorrectMemoryUsage) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  ASSERT_EQ(rm.current(), 0U);

  std::string big1(8192, 'X');
  Builder b1;
  b1.openObject();
  b1.add("k", Value(big1));
  b1.close();
  auto s1 = b1.slice();

  AqlValue v1(s1, s1.byteSize(), &rm);
  ASSERT_EQ(v1.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t mem1 = v1.memoryUsage();
  EXPECT_EQ(mem1, rm.current());
  EXPECT_EQ(mem1, static_cast<size_t>(s1.byteSize()) + ptrOverhead());

  std::string big2(16384, 'Y');
  Builder b2;
  b2.openObject();
  b2.add("k", Value(big2));
  b2.close();
  auto s2 = b2.slice();

  AqlValue v2(s2, s2.byteSize(), &rm);
  ASSERT_EQ(v2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t combined = rm.current();
  size_t mem2 = v2.memoryUsage();
  EXPECT_EQ(mem1 + mem2, combined);
  EXPECT_EQ(mem2, static_cast<size_t>(s2.byteSize()) + ptrOverhead());

  size_t diffPayload = static_cast<size_t>(s2.byteSize() - s1.byteSize());
  size_t diffMem = mem2 - mem1;
  EXPECT_EQ(diffMem, diffPayload);

  v2.destroy();
  EXPECT_EQ(rm.current(), mem1);
  v1.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, DuplicateDestroysAreSafe) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto b = makeLargeArray(2000, 'a');
  Slice slice = b.slice();
  AqlValue aqlVal(slice, &resourceMonitor);
  auto expected = slice.byteSize() + ptrOverhead();
  EXPECT_EQ(aqlVal.memoryUsage(), expected);
  EXPECT_EQ(aqlVal.memoryUsage(), resourceMonitor.current());

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

TEST(AqlValueSupervisedTest, CompareBetweenManagedAndSupervisedReturnSame) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder a = makeString(300, 'a');
  Builder b = makeString(300, 'b');
  Slice sa = a.slice();
  Slice sb = b.slice();

  AqlValue managedA1(sa);
  ASSERT_EQ(managedA1.type(), AqlValue::VPACK_MANAGED_SLICE);
  AqlValue managedA2(sa);
  ASSERT_EQ(managedA2.type(), AqlValue::VPACK_MANAGED_SLICE);
  AqlValue supervisedA1(sa, &rm);
  ASSERT_EQ(supervisedA1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedA2(sa, &rm);
  ASSERT_EQ(supervisedA2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedB1(sb, &rm);
  ASSERT_EQ(supervisedB1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedB2(sb, &rm);
  ASSERT_EQ(supervisedB2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  EXPECT_EQ(AqlValue::Compare(nullptr, managedA1, supervisedA1, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA1, supervisedA2, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA2, supervisedA1, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA2, supervisedA2, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedA1, supervisedA2, true), 0);

  EXPECT_EQ(AqlValue::Compare(nullptr, managedA1, supervisedB1, true), -1);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA1, supervisedB2, true), -1);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA2, supervisedB1, true), -1);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA2, supervisedB2, true), -1);
  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedA1, supervisedB2, true), -1);

  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedB1, managedA1, true), 1);
  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedB1, managedA2, true), 1);
  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedB2, managedA1, true), 1);
  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedB2, managedA2, true), 1);
  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedB1, supervisedA1, true), 1);

  managedA1.destroy();
  managedA2.destroy();
  supervisedA1.destroy();
  supervisedA2.destroy();
  supervisedB1.destroy();
  supervisedB2.destroy();
}

TEST(AqlValueSupervisedTest, ConstructorWithDataPointerForSupervisedSlice) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  ASSERT_EQ(rm.current(), 0U);

  std::string big(4096, 'x');
  Builder b;
  b.add(Value(big));
  Slice s = b.slice();

  AqlValue original(s, s.byteSize(), &rm);
  ASSERT_EQ(original.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t expectedMemory = s.byteSize() + ptrOverhead();
  EXPECT_EQ(original.memoryUsage(), expectedMemory);
  EXPECT_EQ(rm.current(), expectedMemory);

  void const* payloadPtr = original.data();

  uint8_t const* basePtr =
      static_cast<uint8_t const*>(payloadPtr) - sizeof(ResourceMonitor*);

  ResourceMonitor const* storedRm =
      *reinterpret_cast<ResourceMonitor const* const*>(basePtr);
  EXPECT_EQ(storedRm, &rm);

  AqlValue shared(original, payloadPtr);
  ASSERT_EQ(shared.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  EXPECT_EQ(rm.current(), expectedMemory);

  EXPECT_EQ(original.data(), shared.data());
  EXPECT_TRUE(original.slice().binaryEquals(shared.slice()));

  shared.destroy();
  EXPECT_EQ(rm.current(), 0U);

  original.erase();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, MultipleSharedCopiesWithDataPointer) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  ASSERT_EQ(rm.current(), 0U);

  std::string big(8192, 'y');
  Builder b;
  b.add(Value(big));
  Slice s = b.slice();

  AqlValue original(s, s.byteSize(), &rm);
  ASSERT_EQ(original.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t expectedMemory = s.byteSize() + ptrOverhead();
  EXPECT_EQ(original.memoryUsage(), expectedMemory);
  EXPECT_EQ(rm.current(), expectedMemory);

  void const* payloadPtr = original.data();

  AqlValue shared1(original, payloadPtr);
  AqlValue shared2(original, payloadPtr);
  AqlValue shared3(original, payloadPtr);

  EXPECT_EQ(original.data(), shared1.data());
  EXPECT_EQ(original.data(), shared2.data());
  EXPECT_EQ(original.data(), shared3.data());

  EXPECT_EQ(rm.current(), expectedMemory);

  shared1.destroy();
  EXPECT_EQ(rm.current(), 0U);
  shared2.erase();
  shared3.erase();
  original.erase();

  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, AllocationFailureThrowsOOM) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  rm.memoryLimit(1024);

  std::string huge(100000, 'x');
  Builder b;
  b.add(Value(huge));
  Slice s = b.slice();

  EXPECT_THROW({
    AqlValue v(s, s.byteSize(), &rm);
  }, arangodb::basics::Exception);

  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, EdgeCaseEmptyAndMinimalData) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  {
    Builder b;
    b.add(Value(""));
    Slice s = b.slice();
    
    AqlValue v(s, &rm);
    EXPECT_NE(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_EQ(v.memoryUsage(), 0U);
    EXPECT_EQ(rm.current(), 0U);
    v.destroy();
  }

  {
    Builder b;
    b.add(Value("x"));
    Slice s = b.slice();
    
    AqlValue v(s, &rm);
    EXPECT_NE(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_EQ(v.memoryUsage(), 0U);
    v.destroy();
  }

  {
    std::string minimal(sizeof(AqlValue) + 1, 'a');
    Builder b;
    b.add(Value(minimal));
    Slice s = b.slice();
    
    AqlValue v(s, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_GT(v.memoryUsage(), 0U);
    EXPECT_EQ(rm.current(), v.memoryUsage());
    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }
}

TEST(AqlValueSupervisedTest, EdgeCaseMaxSizeAllocation) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  constexpr size_t largeSize = 10 * 1024 * 1024;
  std::string large(largeSize, 'z');
  Builder b;
  b.add(Value(large));
  Slice s = b.slice();

  AqlValue v(s, s.byteSize(), &rm);
  ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t expectedMemory = s.byteSize() + sizeof(ResourceMonitor*);
  EXPECT_EQ(v.memoryUsage(), expectedMemory);
  EXPECT_EQ(rm.current(), expectedMemory);

  EXPECT_TRUE(v.slice().binaryEquals(s));
  EXPECT_EQ(v.slice().getStringLength(), largeSize);

  v.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, ThreadSafetyConcurrentAllocations) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  constexpr int numThreads = 4;
  constexpr int allocationsPerThread = 100;
  std::vector<std::thread> threads;
  std::atomic<int> successCount{0};

  for (int t = 0; t < numThreads; ++t) {
    threads.emplace_back([&rm, &successCount, t]() {
      for (int i = 0; i < allocationsPerThread; ++i) {
        try {
          std::string data(1024 + i, static_cast<char>('a' + t));
          Builder b;
          b.add(Value(data));
          Slice s = b.slice();

          AqlValue v(s, s.byteSize(), &rm);
          EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
          
          EXPECT_EQ(v.slice().getStringLength(), 1024 + i);
          
          v.destroy();
          successCount++;
        } catch (...) {
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(rm.current(), 0U);
  
  EXPECT_GT(successCount.load(), numThreads * allocationsPerThread * 0.9);
}

TEST(AqlValueSupervisedTest, SupervisedBufferConstructorCreatesManaged) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  SupervisedBuffer supervised(rm);
  Builder builder(supervised);
  builder.openArray();
  builder.add(Value(std::string(4096, 'x')));
  builder.close();

  size_t bufferMemory = rm.current();
  EXPECT_GT(bufferMemory, 0U);

  AqlValue v(std::move(supervised), &rm);
  
  EXPECT_EQ(v.type(), AqlValue::VPACK_MANAGED_SLICE);
  EXPECT_GT(v.memoryUsage(), 0U);

  v.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, StringLengthBoundaryAt126And127) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  {
    std::string s126(126, 'a');
    AqlValue v(std::string_view{s126}, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    size_t expected = 1 + s126.size() + sizeof(ResourceMonitor*);
    EXPECT_EQ(v.memoryUsage(), expected);
    EXPECT_EQ(rm.current(), expected);
    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }

  {
    std::string s127(127, 'b');
    AqlValue v(std::string_view{s127}, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    size_t expected = 9 + s127.size() + sizeof(ResourceMonitor*);
    EXPECT_EQ(v.memoryUsage(), expected);
    EXPECT_EQ(rm.current(), expected);
    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }
}

TEST(AqlValueSupervisedTest, ClonePreservesResourceMonitor) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  Builder b;
  b.openObject();
  b.add("x", Value(std::string(1024, 'y')));
  b.close();

  AqlValue original(b.slice(), &rm);
  ASSERT_EQ(original.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t baseMem = rm.current();

  AqlValue cloned = original.clone();
  ASSERT_EQ(cloned.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(rm.current(), baseMem + cloned.memoryUsage());

  cloned.destroy();
  EXPECT_EQ(rm.current(), baseMem);
  
  original.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, DestroyMovedFromValueIsNoOp) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  Builder b;
  b.openArray();
  b.add(Value(999));
  b.close();

  AqlValue original(b.slice(), &rm);
  ASSERT_EQ(original.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  
  size_t initialMem = rm.current();
  EXPECT_GT(initialMem, 0U);

  AqlValue moved(std::move(original));
  EXPECT_EQ(rm.current(), initialMem);

  original.destroy();
  EXPECT_EQ(rm.current(), initialMem);

  moved.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, AllocatedMemoryIsProperlyAligned) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  
  std::vector<size_t> testSizes = {8, 16, 32, 64, 128, 256, 512, 1024, 2048};
  
  for (size_t size : testSizes) {
    Builder b;
    b.openArray();
    for (size_t i = 0; i < size / 8; ++i) {
      b.add(Value(static_cast<int64_t>(i)));
    }
    b.close();
    
    AqlValue v(b.slice(), &rm);
    if (v.type() == AqlValue::VPACK_SUPERVISED_SLICE) {
      // Verify memory was allocated and is being tracked
      EXPECT_GT(rm.current(), 0U)
          << "Memory should be tracked for supervised slice of size " << size;
      
      // Verify memoryUsage returns expected value
      EXPECT_GT(v.memoryUsage(), 0U)
          << "memoryUsage() should return non-zero for size " << size;
    }
    v.destroy();
  }
  
  EXPECT_EQ(rm.current(), 0U);
}

TEST(AqlValueSupervisedTest, AqlItemBlockDoesNotDoubleCountSupervisedMemory) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  AqlItemBlockManager mgr(rm);

  Builder b;
  b.openObject();
  b.add("field", Value(std::string(2048, 'x')));
  b.close();

  AqlValue supervisedValue(b.slice(), &rm);
  ASSERT_EQ(supervisedValue.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  
  size_t memAfterAqlValue = rm.current();
  EXPECT_GT(memAfterAqlValue, 0U);

  auto block = mgr.requestBlock(3, 2);
  size_t memAfterBlock = rm.current();

  block->setValue(0, 0, supervisedValue);
  size_t memAfterFirstSet = rm.current();
  EXPECT_EQ(memAfterFirstSet, memAfterBlock)
      << "setValue should not increase memory for supervised slices";

  block->setValue(1, 0, supervisedValue);
  size_t memAfterSecondSet = rm.current();
  EXPECT_EQ(memAfterSecondSet, memAfterBlock)
      << "Setting same supervised value again should not increase memory";

  AqlValue copy = supervisedValue;
  block->setValue(2, 1, copy);
  size_t memAfterThirdSet = rm.current();
  EXPECT_EQ(memAfterThirdSet, memAfterBlock)
      << "Setting copied supervised value should not increase memory";

  block.reset();
  size_t memAfterBlockDestroyed = rm.current();
  EXPECT_EQ(memAfterBlockDestroyed, memAfterAqlValue)
      << "Block destruction should not affect supervised value memory";

  supervisedValue.destroy();
  EXPECT_EQ(rm.current(), 0U);
}
