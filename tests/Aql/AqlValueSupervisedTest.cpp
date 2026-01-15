#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
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
  // 1) VPACK_INLINE_INT64
  {
    Builder b;
    b.add(Value(42));
    AqlValue v(b.slice());  // Creates VPACK_INLINE_INT64
    EXPECT_EQ(v.type(), AqlValue::VPACK_INLINE_INT64);

    AqlValue cpy = v;
    EXPECT_EQ(cpy.type(), v.type());
    ASSERT_EQ(cpy.memoryUsage(), v.memoryUsage());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    cpy.destroy();
    // v still valid after cpy destroyed
    EXPECT_TRUE(v.slice().isInteger());
    EXPECT_EQ(v.slice().getNumber<int64_t>(), 42);
    v.destroy();
  }

  // 2) VPACK_INLINE_UINT64
  {
    Builder b;
    uint64_t u = (1ULL << 63);  // 9223372036854775808
    b.add(Value(u));
    AqlValue v(b.slice());  // Creates VPACK_INLINE_UINT64
    EXPECT_EQ(v.type(), AqlValue::VPACK_INLINE_UINT64)
        << "type() = " << static_cast<int>(v.type());

    AqlValue cpy = v;
    EXPECT_EQ(cpy.type(), v.type());
    ASSERT_EQ(cpy.memoryUsage(), v.memoryUsage());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    cpy.destroy();
    // v still valid after copy destroyed
    EXPECT_TRUE(v.slice().isInteger());
    EXPECT_EQ(v.slice().getNumber<uint64_t>(), 1ULL << 63);
    v.destroy();
  }

  // 3) VPACK_INLINE_DOUBLE
  {
    Builder b;
    b.add(Value(3.1415926535));
    AqlValue v(b.slice());  // Creates VPACK_INLINE_DOUBLE
    EXPECT_EQ(v.type(), AqlValue::VPACK_INLINE_DOUBLE);

    AqlValue cpy = v;
    EXPECT_EQ(cpy.type(), v.type());
    ASSERT_EQ(cpy.memoryUsage(), v.memoryUsage());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    cpy.destroy();
    // v still valid after copy destroyed
    EXPECT_TRUE(v.slice().isDouble());
    EXPECT_DOUBLE_EQ(v.slice().getNumber<double>(), 3.1415926535);

    v.destroy();
  }

  // 4) VPACK_MANAGED_SLICE
  {
    std::string big(300, 'a');
    arangodb::velocypack::Builder b;
    b.add(arangodb::velocypack::Value(big));

    AqlValue v(b.slice());  // VPACK_MANAGED_SLICE
    ASSERT_EQ(v.type(), AqlValue::VPACK_MANAGED_SLICE);

    // Copy constructor; shallow copy
    AqlValue cpy = v;
    ASSERT_EQ(cpy.type(), AqlValue::VPACK_MANAGED_SLICE);

    // Shallow copy -> pointers are identical, contents are identical
    EXPECT_EQ(cpy.slice().start(), v.slice().start());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    // Destroying copy destroys original's data; v's pointer is dangling
    cpy.destroy();
  }

  // 5) MANAGED_STRING — shallow copy
  {
    Builder b = makeString(300, 'x');
    Slice s = b.slice();
    auto doc = makeDocDataFromSlice(s);
    AqlValue v(doc);  // VPACK_MANAGED_STRING
    ASSERT_EQ(v.type(), AqlValue::VPACK_MANAGED_STRING);

    // Copy ctor; shallow copy
    AqlValue cpy = v;
    ASSERT_EQ(cpy.type(), AqlValue::VPACK_MANAGED_STRING);

    // Same pointers and same contents
    EXPECT_EQ(v.data(), cpy.data());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    // Destroying copy destroys original's data; v's pointer is dangling
    cpy.destroy();
  }

  // 6) SupervisedSlice: copy ctor shallow copy
  {
    auto& g = GlobalResourceMonitor::instance();
    ResourceMonitor rm(g);

    Builder b;
    b.openObject();
    b.add("k", Value(std::string(300, 'a')));
    b.close();
    VPackSlice src = b.slice();

    AqlValue v(src, &rm);  // Original supervised slice
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    auto* pv = v.slice().start();
    std::uint64_t base = rm.current();
    EXPECT_EQ(v.memoryUsage(), base);

    // Copy-ctor -> deep copy
    AqlValue cpy = v;
    ASSERT_EQ(cpy.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    auto* pc = cpy.slice().start();

    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));  // Same content
    EXPECT_EQ(pc, pv);  // Shallow copy should return the same pointer
    EXPECT_EQ(rm.current(),
              base);  // Shallow copy doesn't create a new copy of the obj

    // Destroying copy destroys original's data; v's pointer is dangling
    cpy.destroy();
    EXPECT_EQ(rm.current(), 0U);
    // Use erase() instead of destroy() since v's pointer is dangling
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
  // Small value won't create supervised AqlValue
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

  // Boundary case: allocates in heap
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

  // Shares pointer, creates VPACK_SLICE_POINTER
  AqlValue adopted(slice2.begin());

  // Creating VPACK_SLICE_POINTER won't increase resourceMonitor memory usage
  EXPECT_EQ(resourceMonitor.current(), expected);
  EXPECT_EQ(adopted.memoryUsage(), 0);

  adopted.destroy();
  // resourceMonitor memory usage won't be affected
  EXPECT_EQ(resourceMonitor.current(), expected);

  owned.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

// Test the behavior of AqlValue(uint8_t const* pointer) -> VPACK_SLICE_POINTER
// Even if it takes pointer of SupervisedBuffer, it shouldn't count memory
TEST(AqlValueSupervisedTest, PointerCtorForSupervisedBufferNotAccount) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  // Creates supervisedBuffer obj
  SupervisedBuffer supervised(resourceMonitor);
  Builder builder(supervised);
  builder.openArray();
  builder.add(Value(std::string(1500, 'a')));
  builder.close();

  auto before = resourceMonitor.current();

  AqlValue adopted(builder.slice().begin());  // VPAKC_SLICE_POINTER
  EXPECT_EQ(adopted.memoryUsage(), 0);
  // Memory usage of resourceMonitor should not increase
  EXPECT_EQ(resourceMonitor.current(), before);

  adopted.destroy();
  // Memory usage of resourceMonitor should not decrease
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
  }  // Calls default destructor of copied

  EXPECT_EQ(rm.current(), base);
  original.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Equality tests

// Inline numbers & strings (VPACK_INLINE_* and short strings)
TEST(AqlValueSupervisedTest, InlineNumbersAndStrings) {
  {  // int64
    arangodb::velocypack::Builder b;
    b.add(Value(42));
    AqlValue a(b.slice());
    AqlValue b2(b.slice());
    expectEqualBothWays(a, b2);

    arangodb::velocypack::Builder c;
    c.add(Value(43));
    AqlValue d(c.slice());
    expectNotEqualBothWays(a, d);

    a.destroy();
    b2.destroy();
    d.destroy();
  }

  {  // uint64 (>= 2^63 to ensure it becomes UINT64 inline)
    uint64_t u = (1ULL << 63);
    Builder b;
    b.add(Value(u));
    AqlValue a(b.slice());
    Builder c;
    c.add(Value(u));
    AqlValue a2(c.slice());
    expectEqualBothWays(a, a2);

    Builder d;
    d.add(Value(u + 1));
    AqlValue a3(d.slice());
    expectNotEqualBothWays(a, a3);

    a.destroy();
    a2.destroy();
    a3.destroy();
  }

  {  // double
    Builder b;
    b.add(Value(3.25));
    AqlValue a(b.slice());
    Builder c;
    c.add(Value(3.25));
    AqlValue a2(c.slice());
    expectEqualBothWays(a, a2);

    Builder d;
    d.add(Value(4.0));
    AqlValue a3(d.slice());
    expectNotEqualBothWays(a, a3);

    a.destroy();
    a2.destroy();
    a3.destroy();
  }

  {  // short string (inline slice)
    Builder b;
    b.add(Value("hello"));  // short => inline
    AqlValue a(b.slice());
    Builder c;
    c.add(Value("hello"));
    AqlValue a2(c.slice());
    expectEqualBothWays(a, a2);

    Builder d;
    d.add(Value("world"));
    AqlValue a3(d.slice());
    expectNotEqualBothWays(a, a3);

    a.destroy();
    a2.destroy();
    a3.destroy();
  }
}

// Slice pointer equality: same address == equal; identical bytes at different
// addresses != (by design)
TEST(AqlValueSupervisedTest, SlicePointerAddressSemantics) {
  Builder b;
  b.openArray();
  b.add(Value(1));
  b.close();
  VPackSlice s = b.slice();

  AqlValue p1(s.begin());  // VPACK_SLICE_POINTER
  AqlValue p2(s.begin());  // same pointer
  expectEqualBothWays(p1, p2);

  // Same content, different address -> not equal
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

// Managed slice vs supervised slice with same bytes => equal
TEST(AqlValueSupervisedTest, ManagedSliceEqualsSupervisedSliceSameContent) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b = makeString(300, 'a');  // large => not inline
  Slice s = b.slice();

  AqlValue managed(s);          // VPACK_MANAGED_SLICE
  AqlValue supervised(s, &rm);  // VPACK_SUPERVISED_SLICE

  expectEqualBothWays(managed, supervised);

  // Different content => not equal
  Builder bdiff = makeString(300, 'b');
  AqlValue supervisedDiff(bdiff.slice(), &rm);
  expectNotEqualBothWays(managed, supervisedDiff);

  managed.destroy();
  supervised.destroy();
  supervisedDiff.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Managed string (DocumentData) vs supervised slice with same bytes => equal
TEST(AqlValueSupervisedTest, ManagedStringEqualsSupervisedSliceSameContent) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  // Build large string slice, convert to DocumentData
  Builder bs = makeString(300, 'x');
  Slice ss = bs.slice();
  auto doc = makeDocDataFromSlice(ss);  // creates std::string with VPack bytes

  AqlValue managedString(doc);   // VPACK_MANAGED_STRING
  AqlValue supervised(ss, &rm);  // VPACK_SUPERVISED_SLICE

  expectEqualBothWays(managedString, supervised);

  // Change supervised content -> inequality
  Builder by = makeString(300, 'y');
  AqlValue supervisedY(by.slice(), &rm);
  expectNotEqualBothWays(managedString, supervisedY);

  managedString.destroy();
  supervised.destroy();
  supervisedY.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Supervised slice clone: not equal (pointer comparison, not content)
TEST(AqlValueSupervisedTest, SupervisedSliceCloneEquality) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b = makeLargeArray(512, 'q');  // large array
  AqlValue a(b.slice(), &rm);            // supervised
  AqlValue c =
      a.clone();  // new supervised buffer with same bytes but different pointer

  // Different pointers -> not equal, but content is same
  expectNotEqualBothWays(a, c);
  EXPECT_TRUE(a.slice().binaryEquals(c.slice()));

  a.destroy();
  c.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Range semantics: equality uses pointer identity (not low/high values)
TEST(AqlValueSupervisedTest, RangePointerIdentity) {
  AqlValue r1(1, 3);
  AqlValue r2(1, 3);  // same bounds but distinct object -> NOT equal
  expectNotEqualBothWays(r1, r2);

  // Self equality
  std::equal_to<AqlValue> eq;
  EXPECT_TRUE(eq(r1, r1));

  r1.destroy();
  r2.destroy();
}

// Managed slice shallow copy (copy-ctor) should compare equal (same pointer)
TEST(AqlValueSupervisedTest, ManagedSliceShallowCopyEquality) {
  Builder b = makeString(300, 'm');
  AqlValue v(b.slice());  // managed
  AqlValue cpy = v;       // shallow copy points to same heap block

  // Same pointer -> equal
  expectEqualBothWays(v, cpy);

  // Destroying either invalidates both
  cpy.destroy();
}

// Supervised slice shallow copy (copy-ctor) should compare equal (same pointer)
// NOTE: Copy constructor does a shallow copy, so they share the same pointer
TEST(AqlValueSupervisedTest, SupervisedSliceShallowCopyEquality) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b = makeString(300, 's');
  AqlValue v(b.slice(), &rm);  // SupervisedSlice
  ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue cpy = v;  // shallow copy (shares same pointer)

  expectEqualBothWays(v, cpy);

  cpy.destroy();
  // original AqlValue v's pointer is dangling; we can't call v.destroy()
  v.erase();
  EXPECT_EQ(rm.current(), 0U);
}

// Comprehensive test for behavior of operator==
TEST(AqlValueSupervisedTest, ManagedSliceIsEqualToSupervisedSlice) {
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
  // Cross-type comparisons (managed vs supervised) use content-based equality
  EXPECT_TRUE(eq(managedA1, supervisedA1));
  EXPECT_TRUE(eq(managedA1, supervisedA2));
  EXPECT_TRUE(eq(managedA2, supervisedA1));
  EXPECT_TRUE(eq(managedA2, supervisedA2));

  EXPECT_FALSE(eq(supervisedA1, supervisedA2));  // Different ptrs

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

  std::string big(4096, 'x');  // force supervised allocation

  // Object: correct result, and hasKey must not allocate.
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

  // Non-object: hasKey returns false, and must not allocate.
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

// Test the behavior of at() function
// If bool doCopy = true, creates a copy of AqlValue
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
  // doCopy = false: creates VPACK_SLICE_POINTER, zero memory usage
  AqlValue e0 = a.at(1, mustDestroy, /*doCopy*/ false);
  EXPECT_FALSE(mustDestroy);
  EXPECT_EQ(e0.slice().getStringLength(), 2048);
  EXPECT_EQ(e0.memoryUsage(), 0);
  EXPECT_EQ(rm.current(), originalSize);

  // doCopy = true: creates new supervised slice copy
  AqlValue e1 = a.at(2, mustDestroy, /*doCopy*/ true);
  EXPECT_TRUE(mustDestroy);
  EXPECT_EQ(e1.slice().getStringLength(), 2048);
  EXPECT_GE(e1.memoryUsage(), ptrOverhead() + 2048);
  EXPECT_EQ(rm.current(), originalSize + e1.memoryUsage());

  e1.destroy();
  EXPECT_EQ(rm.current(), originalSize);

  a.destroy();
  EXPECT_EQ(rm.current(), 0);
}

// Test getKeyAttribute(): doCopy=true creates new copy
TEST(AqlValueSupervisedTest, FuncGetKeyAttributeWithDoCopyTrueReturnsCopy) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  // Case 1: Supervised slice WITH _key
  Builder b;
  b.openObject();
  {
    std::string bigKey(10000, 'k');
    b.add("_key", Value(bigKey));
  }
  b.close();
  Slice s = b.slice();

  AqlValue v(s, &rm);  // SupervisedSlice
  EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_TRUE(v.slice().isObject());
  size_t base = rm.current();
  EXPECT_EQ(v.memoryUsage(), base);

  // doCopy = false: returns slice-pointer
  {
    bool mustDestroy = true;
    bool doCopy = false;
    AqlValue out =
        v.getKeyAttribute(mustDestroy, doCopy);  // Returns SlicePointer
    EXPECT_EQ(out.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.slice().isString());
    EXPECT_EQ(out.slice().getStringLength(), s.get("_key").getStringLength());
    EXPECT_TRUE(out.slice().binaryEquals(s.get("_key")));
    out.destroy();  // Does nothing
    EXPECT_EQ(rm.current(),
              base);  // No increase memory since out == SlicePointer
  }

  // doCopy = true: returns supervised copy
  {
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue out =
        v.getKeyAttribute(mustDestroy, doCopy);  // Returns SupervisedSlice
    EXPECT_EQ(out.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(mustDestroy);
    EXPECT_TRUE(out.slice().isString());
    EXPECT_EQ(out.slice().getStringLength(), s.get("_key").getStringLength());
    EXPECT_TRUE(out.slice().binaryEquals(s.get("_key")));

    // Memory increases for new copy
    EXPECT_EQ(rm.current(), base + out.memoryUsage());
    out.destroy();
    EXPECT_EQ(rm.current(), base);
  }

  v.destroy();
  EXPECT_EQ(rm.current(), 0U);

  // Case 2: Supervised slice WITHOUT _key
  Builder b2;
  b2.openObject();
  {
    std::string bigVal(12000, 'x');
    b2.add("x", Value(bigVal));  // no _key
  }
  b2.close();

  Slice s2 = b2.slice();
  ASSERT_TRUE(s2.isObject());
  ASSERT_TRUE(s2.get("_key").isNone());  // no _key

  AqlValue v2(s2, &rm);
  EXPECT_TRUE(v2.slice().isObject());
  base = rm.current();
  EXPECT_EQ(v2.memoryUsage(), base);

  // doCopy = false: returns Null (no key)
  {
    bool mustDestroy = true;
    bool doCopy = false;
    AqlValue out = v2.getKeyAttribute(mustDestroy, doCopy);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.slice().isNull());
    EXPECT_EQ(rm.current(), base);
    out.destroy();  // no operation
    EXPECT_EQ(rm.current(), base);
  }

  // doCopy = true: still no key -> Null
  {
    bool mustDestroy = true;
    bool doCopy = true;
    AqlValue out = v2.getKeyAttribute(mustDestroy, doCopy);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.slice().isNull());
    EXPECT_EQ(rm.current(), base);
    out.destroy();  // no-op
    EXPECT_EQ(rm.current(), base);
  }

  v2.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Test getIdAttribute(): doCopy=true returns new copy
TEST(AqlValueSupervisedTest, FuncGetIdAttributeWithDoCopyTrueReturnsCopy) {
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase = server.getSystemDatabase();
  arangodb::CollectionNameResolver resolver(vocbase);

  auto& global = arangodb::GlobalResourceMonitor::instance();
  arangodb::ResourceMonitor rm(global);
  ASSERT_EQ(rm.current(), 0U);

  // Case 1: Large supervised object WITH "_id"
  Builder b1;
  b1.openObject();
  b1.add("_id", Value("mycol/abcdefhijklmn123456789"));
  b1.close();
  Slice s1 = b1.slice();

  AqlValue v1(s1, &rm);  // SupervisedSlice
  EXPECT_EQ(v1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  size_t base1 = rm.current();
  EXPECT_EQ(base1, v1.memoryUsage());

  // doCopy = false: returns SlicePointer
  {
    bool mustDestroy = true;
    bool doCopy = false;
    AqlValue out = v1.getIdAttribute(resolver, mustDestroy, doCopy);
    EXPECT_EQ(out.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.slice().isString());
    EXPECT_TRUE(out.slice().binaryEquals(s1.get("_id")));

    EXPECT_EQ(rm.current(), base1);  // No increase memory
    out.destroy();                   // no-op
    EXPECT_EQ(rm.current(), base1);  // No change
  }

  // doCopy = true: produces supervised copy
  {
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue out = v1.getIdAttribute(resolver, mustDestroy, doCopy);
    EXPECT_EQ(out.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(mustDestroy);
    EXPECT_TRUE(out.slice().isString());
    EXPECT_TRUE(out.slice().binaryEquals(s1.get("_id")));

    EXPECT_EQ(rm.current(), base1 + out.memoryUsage());
    out.destroy();
    EXPECT_EQ(rm.current(), base1);
  }

  // Case 2: Large supervised object WITHOUT "_id"
  Builder b2;
  b2.openObject();
  b2.add("big", arangodb::velocypack::Value(std::string(16000, 'y')));
  b2.close();
  Slice s2 = b2.slice();
  ASSERT_TRUE(s2.get("_id").isNone());

  AqlValue v2(s2, &rm);
  EXPECT_EQ(v2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  size_t base2 = rm.current();
  EXPECT_EQ(base2, base1 + v2.memoryUsage());  // v1 + v2 accounted

  // doCopy = false: no _id -> returns Null
  {
    bool mustDestroy = true;
    bool doCopy = false;
    AqlValue out = v2.getIdAttribute(resolver, mustDestroy, doCopy);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.slice().isNull());

    EXPECT_EQ(rm.current(), base2);  // no increase memory
    out.destroy();                   // no-op
    EXPECT_EQ(rm.current(), base2);  // unchanged
  }

  // doCopy = true: still no _id -> Null
  {
    bool mustDestroy = true;
    bool doCopy = true;
    size_t before = rm.current();
    AqlValue out = v2.getIdAttribute(resolver, mustDestroy, doCopy);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.slice().isNull());

    EXPECT_EQ(rm.current(), base2);   // no increase memory
    out.destroy();                    // no-op
    EXPECT_EQ(rm.current(), before);  // unchanged
  }

  v2.destroy();
  v1.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Test getFromAttribute/getToAttribute(): doCopy=true returns new copy
TEST(AqlValueSupervisedTest, FuncGetFromAndToAttributesReturnCopy) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  const std::string big(4096, 'x');
  const std::string fromValue = "vertices/users/" + big;
  const std::string toValue = "vertices/posts/12345-" + big;

  // Document WITH both _from and _to
  Builder bEdge;
  bEdge.openObject();
  bEdge.add("_from", Value(fromValue));
  bEdge.add("_to", Value(toValue));
  bEdge.add("note", Value(big));  // keep object large
  bEdge.close();

  AqlValue src(Slice(bEdge.slice()), &rm);
  ASSERT_EQ(src.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  auto base = rm.current();

  // getFromAttribute: doCopy = false
  {
    bool mustDestroy = false;
    AqlValue out = src.getFromAttribute(mustDestroy, /*doCopy*/ false);

    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.isString());
    EXPECT_EQ(out.slice().copyString(), fromValue);
    EXPECT_EQ(rm.current(), base);  // no additional memory
  }

  // getFromAttribute: doCopy = true
  {
    bool mustDestroy = false;
    AqlValue out = src.getFromAttribute(mustDestroy, /*doCopy*/ true);

    EXPECT_TRUE(mustDestroy);
    EXPECT_TRUE(out.isString());
    EXPECT_EQ(out.slice().copyString(), fromValue);

    // Memory accounted for the copy
    EXPECT_EQ(rm.current(), base + out.memoryUsage());

    out.destroy();
    EXPECT_EQ(rm.current(), base);  // back to base after destroy
  }

  // getToAttribute: doCopy = false
  {
    bool mustDestroy = false;
    AqlValue out = src.getToAttribute(mustDestroy, /*doCopy*/ false);

    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.isString());
    EXPECT_EQ(out.slice().copyString(), toValue);
    EXPECT_EQ(rm.current(), base);  // no additional memory
  }

  // getToAttribute: doCopy = true
  {
    bool mustDestroy = false;
    AqlValue out = src.getToAttribute(mustDestroy, /*doCopy*/ true);

    EXPECT_TRUE(mustDestroy);
    EXPECT_TRUE(out.isString());
    EXPECT_EQ(out.slice().copyString(), toValue);

    // Memory accounted for the copy
    EXPECT_EQ(rm.current(), base + out.memoryUsage());

    out.destroy();
    EXPECT_EQ(rm.current(), base);  // back to base after destroy
  }

  src.destroy();
  EXPECT_EQ(rm.current(), 0U);

  // Document WITHOUT _from and _to
  Builder bNoAttrs;
  bNoAttrs.openObject();
  bNoAttrs.add("foo", Value(1));
  bNoAttrs.add("bar", Value(big));  // still large, but no _from/_to
  bNoAttrs.close();

  AqlValue srcNoAttrs(Slice(bNoAttrs.slice()), &rm);
  ASSERT_EQ(srcNoAttrs.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  base = rm.current();

  // getFromAttribute: missing -> null
  {
    bool mustDestroy = false;
    AqlValue out = srcNoAttrs.getFromAttribute(mustDestroy, /*doCopy*/ true);

    EXPECT_FALSE(mustDestroy);
    EXPECT_EQ(out.getTypeString(), "null");
    EXPECT_EQ(rm.current(), base);  // no memory increase
  }

  // getToAttribute: missing -> null
  {
    bool mustDestroy = false;
    AqlValue out = srcNoAttrs.getToAttribute(mustDestroy, /*doCopy*/ true);

    EXPECT_FALSE(mustDestroy);
    EXPECT_EQ(out.getTypeString(), "null");
    EXPECT_EQ(rm.current(), base);  // no memory increase
  }

  srcNoAttrs.destroy();
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

  // 1) get(by name), doCopy = false
  {
    bool mustDestroy = false;
    bool doCopy = false;
    AqlValue got = v.get(resolver, "payload", mustDestroy, doCopy);
    EXPECT_EQ(got.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_FALSE(mustDestroy);
    ASSERT_FALSE(got.isNone());
    ASSERT_TRUE(got.slice().isString());

    EXPECT_EQ(rm.current(), baseMem);  // No increase memory
    got.destroy();                     // no-op
    EXPECT_EQ(rm.current(), baseMem);  // No change
  }

  // 2) get(by name), doCopy = true
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

  // 3) get(by list), doCopy = false
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

  // 4) get(by list), doCopy = true
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

  // 5) get(by name) missing field -> Null
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

  // 6) get(by path) missing mid-path -> Null
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

// Test data() returns pointer to heap data (not ResourceMonitor*)
TEST(AqlValueSupervisedTest, FuncDataReturnsPointerToActualData) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  // 1) SupervisedSlice
  Builder b1 = makeString(300, 'a');
  Slice s1 = b1.slice();

  AqlValue a1(s1, &rm);  // supervised slice
  auto e1 = s1.byteSize() + ptrOverhead();
  EXPECT_EQ(a1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(a1.memoryUsage(), e1);
  EXPECT_EQ(a1.memoryUsage(), rm.current());

  // data() points to slice payload, not ResourceMonitor*
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

// Test materialize() default case
TEST(AqlValueSupervisedTest,
     FuncMaterializeForSupervisedAqlValueReturnsShallowCopy) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  // Non-range SupervisedSlice
  std::string big(8 * 1024, 'x');
  Builder obj;
  obj.openObject();
  obj.add("q", Value(7));
  obj.add("big", Value(big));
  obj.close();

  AqlValue v1(obj.slice(), &rm);  // SupervisedSlice
  std::uint64_t base = rm.current();

  bool copied1 = true;
  AqlValue mat1 = v1.materialize(nullptr, copied1);
  // materialize() uses default copy ctor (shallow copy)
  EXPECT_FALSE(copied1);

  EXPECT_TRUE(v1.slice().binaryEquals(obj.slice()));
  EXPECT_TRUE(mat1.slice().binaryEquals(obj.slice()));

  // Shallow copy: no memory increase
  EXPECT_EQ(rm.current(), base);

  mat1.destroy();
  EXPECT_EQ(rm.current(), 0U);

  // v1's pointer is dangling; use erase()
  v1.erase();
  EXPECT_EQ(rm.current(), 0U);
}

// Test if slice() returns slice of actual data
TEST(AqlValueSupervisedTest,
     FuncSliceWithAqlValueTypeReturnsSliceOfActualData) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  // SUPERVISED_SLICE
  Builder b1 = makeString(300, 'a');
  Slice s1 = b1.slice();
  AqlValue a1(s1, &rm);  // supervised slice
  EXPECT_EQ(a1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  {
    auto sl = a1.slice();
    EXPECT_EQ(sl.getStringLength(), 300);
    // Points to actual payload
    EXPECT_EQ(sl.start(), static_cast<uint8_t const*>(a1.data()));
  }

  a1.destroy();
  EXPECT_EQ(rm.current(), 0u);
}

// Test clone() creates new copy (doubles memory usage)
TEST(AqlValueSupervisedTest, FuncCloneCreatesAnotherCopy) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  Builder builder;
  builder.openArray();
  builder.add(Value(std::string(1024, 'a')));
  builder.close();
  Slice slice = builder.slice();

  AqlValue aqlVal(slice, &resourceMonitor);  // SUPERVISED_SLICE
  AqlValue cloneVal = aqlVal.clone();
  // Independent AqlValues: memory usage doubled
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

  // Construct second, larger supervised slice
  std::string big2(16384, 'Y');  // larger payload
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

// Test duplicate destroy() calls are safe
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

  // destroy() calls erase() which zeros the 16 bytes
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

  // Same content
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA1, supervisedA1, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA1, supervisedA2, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA2, supervisedA1, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA2, supervisedA2, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedA1, supervisedA2, true), 0);

  // Different content
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

// Test AqlValue(AqlValue const&, void const*) for supervised slices
// Used when sharing values between blocks. 'data' is payload pointer;
// constructor adjusts to base pointer (includes ResourceMonitor* prefix).
TEST(AqlValueSupervisedTest, ConstructorWithDataPointerForSupervisedSlice) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  ASSERT_EQ(rm.current(), 0U);

  // Create a supervised slice with a large string
  std::string big(4096, 'x');
  Builder b;
  b.add(Value(big));
  Slice s = b.slice();

  // Create original supervised AqlValue
  AqlValue original(s, s.byteSize(), &rm);
  ASSERT_EQ(original.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  size_t expectedMemory = s.byteSize() + ptrOverhead();
  EXPECT_EQ(original.memoryUsage(), expectedMemory);
  EXPECT_EQ(rm.current(), expectedMemory);

  // Get the payload pointer (this is what data() returns)
  void const* payloadPtr = original.data();

  // Verify that payloadPtr is offset by kPrefix from the base
  // The base pointer should be at payloadPtr - sizeof(ResourceMonitor*)
  uint8_t const* basePtr =
      static_cast<uint8_t const*>(payloadPtr) - sizeof(ResourceMonitor*);

  // Verify we can read the ResourceMonitor* from the base
  ResourceMonitor const* storedRm =
      *reinterpret_cast<ResourceMonitor const* const*>(basePtr);
  EXPECT_EQ(storedRm, &rm);

  // Now use the AqlValue(AqlValue const&, void const*) constructor
  // This simulates what happens in cloneDataAndMoveShadow when it calls
  // AqlValue(a, (*it)) where (*it) is the payload pointer from cache
  AqlValue shared(original, payloadPtr);
  ASSERT_EQ(shared.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  // Memory should still be the same (sharing the same allocation)
  EXPECT_EQ(rm.current(), expectedMemory);

  // Both should point to the same data
  EXPECT_EQ(original.data(), shared.data());
  EXPECT_TRUE(original.slice().binaryEquals(shared.slice()));

  // Destroying shared copy should free memory (they share same allocation)
  shared.destroy();
  EXPECT_EQ(rm.current(), 0U);

  // Original's pointer is dangling; use erase() instead of destroy()
  original.erase();
  EXPECT_EQ(rm.current(), 0U);
}

// Test the same scenario but with multiple shared copies to ensure
// the fix works correctly in the reference counting scenario
TEST(AqlValueSupervisedTest, MultipleSharedCopiesWithDataPointer) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  ASSERT_EQ(rm.current(), 0U);

  // Create a supervised slice
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

  // Create multiple shared copies (simulates AqlItemBlock sharing)
  AqlValue shared1(original, payloadPtr);
  AqlValue shared2(original, payloadPtr);
  AqlValue shared3(original, payloadPtr);

  // All should point to the same data
  EXPECT_EQ(original.data(), shared1.data());
  EXPECT_EQ(original.data(), shared2.data());
  EXPECT_EQ(original.data(), shared3.data());

  // Memory should still be the same (one allocation shared by all)
  EXPECT_EQ(rm.current(), expectedMemory);

  // Destroying first copy frees memory; others have dangling pointers
  shared1.destroy();
  EXPECT_EQ(rm.current(), 0U);
  shared2.erase();
  shared3.erase();
  original.erase();

  EXPECT_EQ(rm.current(), 0U);
}

// Test OOM during supervised allocation
TEST(AqlValueSupervisedTest, AllocationFailureThrowsOOM) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  rm.memoryLimit(1024);

  // Try to allocate a huge supervised slice that exceeds the limit
  std::string huge(100000, 'x');
  Builder b;
  b.add(Value(huge));
  Slice s = b.slice();

  // Should throw TRI_ERROR_RESOURCE_LIMIT
  EXPECT_THROW({
    AqlValue v(s, s.byteSize(), &rm);
  }, arangodb::basics::Exception);

  EXPECT_EQ(rm.current(), 0U);
}

// Test edge case: empty/minimal data
TEST(AqlValueSupervisedTest, EdgeCaseEmptyAndMinimalData) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  // Empty string (will be inline, not supervised)
  {
    Builder b;
    b.add(Value(""));
    Slice s = b.slice();
    
    AqlValue v(s, &rm);
    // Empty strings are inline
    EXPECT_NE(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_EQ(v.memoryUsage(), 0U);
    EXPECT_EQ(rm.current(), 0U);
    v.destroy();
  }

  // Single-byte string (still inline)
  {
    Builder b;
    b.add(Value("x"));
    Slice s = b.slice();
    
    AqlValue v(s, &rm);
    EXPECT_NE(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_EQ(v.memoryUsage(), 0U);
    v.destroy();
  }

  // Minimal supervised slice (just over inline threshold)
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

// Test edge case: maximum size allocation
TEST(AqlValueSupervisedTest, EdgeCaseMaxSizeAllocation) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  // Allocate a very large supervised slice (10MB)
  constexpr size_t largeSize = 10 * 1024 * 1024;
  std::string large(largeSize, 'z');
  Builder b;
  b.add(Value(large));
  Slice s = b.slice();

  AqlValue v(s, s.byteSize(), &rm);
  ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  // Verify memory accounting for large allocation
  size_t expectedMemory = s.byteSize() + sizeof(ResourceMonitor*);
  EXPECT_EQ(v.memoryUsage(), expectedMemory);
  EXPECT_EQ(rm.current(), expectedMemory);

  // Verify data integrity
  EXPECT_TRUE(v.slice().binaryEquals(s));
  EXPECT_EQ(v.slice().getStringLength(), largeSize);

  v.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Test thread safety: concurrent supervised allocations
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
          
          // Verify data integrity
          EXPECT_EQ(v.slice().getStringLength(), 1024 + i);
          
          v.destroy();
          successCount++;
        } catch (...) {
          // Allocation failures are acceptable under high concurrency
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all allocations were properly cleaned up
  EXPECT_EQ(rm.current(), 0U);
  
  // Verify most allocations succeeded
  EXPECT_GT(successCount.load(), numThreads * allocationsPerThread * 0.9);
}
