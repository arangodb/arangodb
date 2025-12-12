#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/SupervisedBuffer.h"
#include "Mocks/Servers.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/Slice.h>
#include <velocypack/Buffer.h>

#include "Logger/LogMacros.h"

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
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

inline void expectNotEqualBothWays(AqlValue const& a, AqlValue const& b) {
  std::equal_to<AqlValue> eq;
  EXPECT_FALSE(eq(a, b));
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}
}  // namespace

// Test for AqlValue(string_view, ResourceMonitor*) <- short str
TEST(AqlValueSupervisedTest, ShortStringViewCtorAccountsCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  arangodb::ResourceMonitor rm(global);

  std::string s(15, 'x');  // 15 chars, fits short str (<= 126), not inline
  auto payloadSize = 1 + s.size();  // tag&len (= 1) + chars
  auto expected = ptrOverhead() + payloadSize;

  AqlValue v(std::string_view{s}, &rm);

  // checks correct counting with internal logic
  EXPECT_EQ(v.memoryUsage(), expected);
  // checks correct counting with external logic
  EXPECT_EQ(v.memoryUsage(), rm.current());

  // Slice should decode back to original string
  auto slice = v.slice();
  ASSERT_TRUE(slice.isString());
  EXPECT_EQ(slice.stringView(), s);

  v.destroy();
  // After destruction, usage returns to 0
  EXPECT_EQ(rm.current(), 0);
}

// Test for AqlValue(string_view, ResourceMonitor* nullptr) <- long str
TEST(AqlValueSupervisedTest, LongStringViewCtorAccountsCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  arangodb::ResourceMonitor rm(global);

  std::string s(300, 'A');              // triggers long string encoding
  auto payloadSize = 1 + 8 + s.size();  // len + tag + chars
  std::size_t expected = ptrOverhead() + payloadSize;

  AqlValue v(std::string_view{s}, &rm);

  // checks correct counting with internal logic
  EXPECT_EQ(v.memoryUsage(), expected);
  // checks correct counting with external logic
  EXPECT_EQ(v.memoryUsage(), rm.current());

  // Slice should decode back to original string
  auto slice = v.slice();
  ASSERT_TRUE(slice.isString());
  EXPECT_EQ(slice.stringView(), s);

  v.destroy();
  // After destruction, usage returns to 0
  EXPECT_EQ(rm.current(), 0);
}

// Test for AqlValue(Buffer&&, ResourceMonitor* = nullptr)
TEST(AqlValueSupervisedTest, BufferMoveCtorAccountsCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto builder = makeLargeArray(2048, 'a');
  Slice slice = builder.slice();
  velocypack::Buffer<uint8_t> buffer;
  buffer.append(slice.start(), slice.byteSize());

  AqlValue aqlVal(std::move(buffer), &resourceMonitor);
  EXPECT_EQ(aqlVal.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  // Check that the original buffer is now empty after move
  EXPECT_EQ(buffer.size(), 0U);
  EXPECT_TRUE(buffer.empty());

  auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
  // checks correct counting with internal logic
  EXPECT_EQ(aqlVal.memoryUsage(), expected);
  // checks correct counting with external logic
  EXPECT_EQ(aqlVal.memoryUsage(), resourceMonitor.current());

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

// Test for AqlValue(Buffer&, ResourceMonitor* = nullptr)
TEST(AqlValueSupervisedTest, BufferCtorAccountsCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto builder = makeLargeArray(2048, 'a');
  Slice slice = builder.slice();
  velocypack::Buffer<uint8_t> buffer;
  buffer.append(slice.start(), slice.byteSize());
  auto before = buffer.size();

  AqlValue aqlVal(buffer, &resourceMonitor);
  EXPECT_EQ(aqlVal.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  // Buffer obj itself doesn't change
  EXPECT_EQ(buffer.size(), before);

  auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
  // checks correct counting with internal logic
  EXPECT_EQ(aqlVal.memoryUsage(), expected);
  // checks correct counting with external logic
  EXPECT_EQ(aqlVal.memoryUsage(), resourceMonitor.current());

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

// Test for AqlValue(Slice, ResourceMonitor* = nullptr)
TEST(AqlValueSupervisedTest, SliceCtorAccountsCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto builder = makeLargeArray(1024, 'a');
  Slice slice = builder.slice();
  AqlValue aqlVal(slice, static_cast<ValueLength>(slice.byteSize()),
                  &resourceMonitor);

  auto expected = static_cast<size_t>(slice.byteSize() + ptrOverhead());
  // checks correct counting with internal logic
  EXPECT_EQ(aqlVal.memoryUsage(), expected);
  // checks correct counting with external logic
  EXPECT_EQ(aqlVal.memoryUsage(), resourceMonitor.current());

  EXPECT_TRUE(aqlVal.slice().isArray());
  {
    ValueLength l = 0;
    (void)aqlVal.slice().at(0).getStringUnchecked(l);
    EXPECT_EQ(l, 1024);
  }

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

// Test for AqlValue(Slice, ResourceMonitor* = nullptr)
// This creates multiple supervised AqlValues
TEST(AqlValueSupervisedTest, MultipleSliceCtorAccountsCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto bA = makeLargeArray(1000, 'a');
  auto bB = makeLargeArray(2000, 'b');
  auto bC = makeLargeArray(3000, 'c');
  Slice sliceA = bA.slice();
  Slice sliceB = bB.slice();
  Slice sliceC = bC.slice();

  AqlValue a1(sliceA, 0, &resourceMonitor);  // SUPERVISED_SLICE
  AqlValue a2(sliceB, 0, &resourceMonitor);  // SUPERVISED_SLICE
  AqlValue a3(sliceC, 0, &resourceMonitor);  // SUPERVISED_SLICE

  size_t e1 = static_cast<size_t>(sliceA.byteSize()) + ptrOverhead();
  size_t e2 = static_cast<size_t>(sliceB.byteSize()) + ptrOverhead();
  size_t e3 = static_cast<size_t>(sliceC.byteSize()) + ptrOverhead();

  // checks correct counting with internal logic
  EXPECT_EQ(a1.memoryUsage() + a2.memoryUsage() + a3.memoryUsage(),
            e1 + e2 + e3);
  // checks correct counting with external logic
  EXPECT_EQ(a1.memoryUsage() + a2.memoryUsage() + a3.memoryUsage(),
            resourceMonitor.current());

  a1.destroy();
  EXPECT_EQ(resourceMonitor.current(), e2 + e3);

  a2.destroy();
  EXPECT_EQ(resourceMonitor.current(), e3);

  a3.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

// Test for AqlValue(AqlValueHintSliceCopy, ResourceMonitor* = nullptr)
TEST(AqlValueSupervisedTest, HintSliceCopyCtorAccountsCorrectSize) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto builder = makeLargeArray(512, 'a');
  Slice slice = builder.slice();
  AqlValueHintSliceCopy hint{slice};
  AqlValue aqlVal(hint, &resourceMonitor);

  auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
  // checks correct counting with internal logic
  EXPECT_EQ(aqlVal.memoryUsage(), expected);
  // checks correct counting with external logic
  EXPECT_EQ(aqlVal.memoryUsage(), resourceMonitor.current());

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);
}

// Checks the behavior of copy constructor AqlValue(AqlValue const other&)
// For supervised AqlValue,
// it will create a new heap obj.
// Other than them, it won't create a new heap obj i.e. shallow copy
TEST(AqlValueSupervisedTest, CopyCtorAccountsCorrectSize) {
  // 1) VPACK_INLINE_INT64
  {
    Builder b;
    b.add(Value(42));
    AqlValue v(b.slice());  // Creates VPACK_INLINE_INT64
    EXPECT_EQ(v.type(), AqlValue::VPACK_INLINE_INT64);

    // Invokes copy constructor; creates another copy of the AqlValue
    AqlValue cpy = v;
    EXPECT_EQ(cpy.type(), v.type());
    ASSERT_EQ(cpy.memoryUsage(), v.memoryUsage());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    cpy.destroy();
    // Checks v is still alive after cpy is destroyed
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

    // Copy constructor
    AqlValue cpy = v;
    EXPECT_EQ(cpy.type(), v.type());
    ASSERT_EQ(cpy.memoryUsage(), v.memoryUsage());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    cpy.destroy();
    // Ensure v is still alive after destroying copy
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

    // Copy constructor
    AqlValue cpy = v;
    EXPECT_EQ(cpy.type(), v.type());
    ASSERT_EQ(cpy.memoryUsage(), v.memoryUsage());
    EXPECT_TRUE(cpy.slice().binaryEquals(v.slice()));

    cpy.destroy();
    // Ensure v is still alive after destroying copy
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

    // Destroying the copy will destroy the original's heap data too
    // BE AWARE: v's pointer is dangling
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

    // Destroy the copy; the original's heap data is also destroyed
    // BE AWARE: v's pointer is dangling
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

    AqlValue v(src, /*length*/ 0, &rm);  // Original supervised slice
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

    // Destroy the copy -> original AqlValue's heap data is also destroyed
    cpy.destroy();
    EXPECT_EQ(rm.current(), 0U);

    // We cannot call v.destroy() here because v's pointer is dangling!
    // Instead call v.erase() to zero out the v's 16 bytes
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
  AqlValue a(Slice(bin.slice()), /*byteSize*/ 0, &rm);  // SupervisedSlice
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

  // Destroy the owner; accounting returns to previous level (0 here)
  c.destroy();
  // Cannot call a.destroy() or b.destroy() because their pointers are dangling
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
  // Passing resourceMonitor*, but won't create a supervised AqlValue
  AqlValue aqlVal(slice, 0, &resourceMonitor);

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

  // This is the very boundary when AqlValue allocates the data in the heap
  auto builder = makeString(16, 'a');
  Slice slice = builder.slice();
  AqlValue aqlVal(slice, 0, &resourceMonitor);

  auto expected = static_cast<size_t>(slice.byteSize()) + ptrOverhead();
  // checks correct counting with internal logic
  EXPECT_EQ(aqlVal.memoryUsage(), expected);
  // checks correct counting with external logic
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
  AqlValue owned(slice1, 0, &resourceMonitor);
  ASSERT_EQ(owned.memoryUsage(), expected);
  ASSERT_EQ(owned.memoryUsage(), resourceMonitor.current());

  // This just shares the pointer, won't create new obj.
  // Creates VPACK_SLICE_POINTER
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
  AqlValue original(slice, 0, &rm);
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

// ====================== Equality tests ======================
// Test operator== for all the AqlValueType

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

  AqlValue managed(s);             // VPACK_MANAGED_SLICE
  AqlValue supervised(s, 0, &rm);  // VPACK_SUPERVISED_SLICE

  expectEqualBothWays(managed, supervised);

  // Different content => not equal
  Builder bdiff = makeString(300, 'b');
  AqlValue supervisedDiff(bdiff.slice(), 0, &rm);
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

  // Build a large string slice, then turn it into DocumentData
  Builder bs = makeString(300, 'x');
  Slice ss = bs.slice();
  auto doc = makeDocDataFromSlice(ss);  // creates std::string with VPack bytes

  AqlValue managedString(doc);      // VPACK_MANAGED_STRING
  AqlValue supervised(ss, 0, &rm);  // VPACK_SUPERVISED_SLICE

  expectEqualBothWays(managedString, supervised);

  // Change supervised content -> inequality
  Builder by = makeString(300, 'y');
  AqlValue supervisedY(by.slice(), 0, &rm);
  expectNotEqualBothWays(managedString, supervisedY);

  managedString.destroy();
  supervised.destroy();
  supervisedY.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Supervised slice self-clone should remain equal (bytewise equal)
TEST(AqlValueSupervisedTest, SupervisedSliceCloneEquality) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b = makeLargeArray(512, 'q');  // large array
  AqlValue a(b.slice(), 0, &rm);         // supervised
  AqlValue c = a.clone();  // new supervised buffer with same bytes

  expectEqualBothWays(a, c);

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
  EXPECT_TRUE(r1 == r1);
  EXPECT_FALSE(r1 != r1);

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

  // NOTE: destroying either invalidates both; don't access v after destroying
  // cpy.
  cpy.destroy();
}

// Supervised slice deep copy (copy-ctor) should compare equal (bytewise)
TEST(AqlValueSupervisedTest, SupervisedSliceShallowCopyEquality) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  Builder b = makeString(300, 's');
  AqlValue v(b.slice(), 0, &rm);  // SupervisedSlice
  ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue cpy = v;  // shallow copy

  expectEqualBothWays(v, cpy);

  cpy.destroy();
  // original AqlValue v's pointer is dangling; we can't call v.destroy()
  v.erase();
  EXPECT_EQ(rm.current(), 0U);
}

// Comprehensive test for behavior of operator==; ManagedSlice should be equal
// to SupervisedSlice as long as the heap data are the same
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
  AqlValue supervisedA1(sa, 0, &rm);
  ASSERT_EQ(supervisedA1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedA2(sa, 0, &rm);
  ASSERT_EQ(supervisedA2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedB1(sb, 0, &rm);
  ASSERT_EQ(supervisedB1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedB2(sb, 0, &rm);
  ASSERT_EQ(supervisedB2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  std::equal_to<AqlValue> eq;
  // They should be equal
  EXPECT_TRUE(eq(managedA1, supervisedA1));
  EXPECT_TRUE(eq(managedA1, supervisedA2));
  EXPECT_TRUE(eq(managedA2, supervisedA1));
  EXPECT_TRUE(eq(managedA2, supervisedA2));

  EXPECT_TRUE(eq(supervisedA1, supervisedA2));

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
  AqlValue supSlice(b.slice(), 0, &rm);
  EXPECT_EQ(supSlice.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_TRUE(supSlice.requiresDestruction());  // This should return true

  supSlice.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Test behavior of predicate functions
// isString(), isObject(), isArray()
TEST(AqlValueSupervisedTest, PredicateFuncsReturnCorrectValue) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  std::string big(4096, 'x');
  // ---------- string (large) ----------
  {
    Builder b;
    b.add(Value(big));
    AqlValue v(Slice(b.slice()), 0, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_FALSE(v.isNumber());
    EXPECT_TRUE(v.isString());
    EXPECT_FALSE(v.isObject());
    EXPECT_FALSE(v.isArray());
    v.destroy();
  }

  // ---------- object (large) ----------
  {
    Builder b;
    b.openObject();
    b.add("a", Value(1));
    b.add("b", Value(2.0));
    b.add("c", Value(big));
    for (int i = 0; i < 16; ++i) {
      b.add(std::string("k") + std::to_string(i), Value(std::string(256, 'y')));
    }
    b.close();

    AqlValue v(Slice(b.slice()), 0, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_FALSE(v.isNumber());
    EXPECT_FALSE(v.isString());
    EXPECT_TRUE(v.isObject());
    EXPECT_FALSE(v.isArray());
    v.destroy();
  }

  // ---------- array (large) ----------
  {
    Builder b;
    b.openArray();
    b.add(Value(42));
    b.add(Value(3.14));
    b.add(Value(big));
    for (int i = 0; i < 64; ++i) {
      b.add(Value(std::string(128, 'z')));
    }
    b.close();

    AqlValue v(Slice(b.slice()), 0, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_FALSE(v.isNumber());
    EXPECT_FALSE(v.isString());
    EXPECT_FALSE(v.isObject());
    EXPECT_TRUE(v.isArray());
    v.destroy();
  }
}

// Test behavior of hasKey() function for SupervisedSlice
TEST(AqlValueSupervisedTest, HasKeyFuncForSupervisedSliceReturnsCorrectValue) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  std::string big(4096, 'x');

  // ----- Object case: should answer true/false correctly -----
  {
    Builder b;
    b.openObject();
    b.add("a", Value(1));
    b.add("b", Value(2.0));
    b.add("long", Value(big));
    b.add("nested", Value("value"));
    b.close();

    AqlValue v(Slice(b.slice()), 0, &rm);  // SupervisedSlice
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    EXPECT_TRUE(v.hasKey("a"));
    EXPECT_TRUE(v.hasKey("b"));
    EXPECT_TRUE(v.hasKey("long"));
    EXPECT_TRUE(v.hasKey("nested"));

    EXPECT_FALSE(v.hasKey("missing"));
    EXPECT_FALSE(v.hasKey(""));

    v.destroy();
  }

  // ----- Non-object (array): hasKey must be false -----
  {
    Builder b;
    b.openArray();
    b.add(Value(1));
    b.add(Value(big));
    b.add(Value(3));
    b.close();

    AqlValue v(Slice(b.slice()), 0, &rm);  // SupervisedSlice
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    EXPECT_FALSE(v.hasKey("a"));
    EXPECT_FALSE(v.hasKey("0"));
    v.destroy();
  }
}

// Test behavior of getTypeString() function for SupervisedSlice
// This function returns the actual type (not AqlValueType) of the heap data
TEST(AqlValueSupervisedTest,
     FuncGetTypeStringFuncForSupervisedSliceReturnsCorrectValue) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);
  std::string big(4096, 'x');

  // --- string (large) ---
  {
    Builder b;
    b.add(Value(big));
    AqlValue v(Slice(b.slice()), 0, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_EQ(v.getTypeString(), "string");
    v.destroy();
  }

  // --- object (large) ---
  {
    Builder b;
    b.openObject();
    b.add("a", Value(1));
    b.add("b", Value(2.0));
    b.add("c", Value(big));
    for (int i = 0; i < 8; ++i) {
      b.add(std::string("k") + std::to_string(i), Value(std::string(256, 'y')));
    }
    b.close();

    AqlValue v(Slice(b.slice()), 0, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_EQ(v.getTypeString(), "object");
    v.destroy();
  }

  // --- array (large) ---
  {
    Builder b;
    b.openArray();
    b.add(Value(1));
    b.add(Value(2.0));
    b.add(Value(big));  // ensure array is large
    for (int i = 0; i < 32; ++i) {
      b.add(Value(std::string(128, 'z')));
    }
    b.close();

    AqlValue v(Slice(b.slice()), 0, &rm);
    EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_EQ(v.getTypeString(), "array");
    v.destroy();
  }
  EXPECT_EQ(rm.current(), 0);
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

  AqlValue a(arr.slice(), 0, &rm);
  auto originalSize = arr.slice().byteSize() + ptrOverhead();
  ASSERT_EQ(a.memoryUsage(), originalSize);
  ASSERT_EQ(a.memoryUsage(), rm.current());

  bool mustDestroy = false;
  // doCopy = false -> NOT create copy, mustDestroy remains false
  // This creates VPACK_SLICE_POINTER AqlValue -> zero memory usage
  AqlValue e0 = a.at(1, mustDestroy, /*doCopy*/ false);
  EXPECT_FALSE(mustDestroy);
  EXPECT_EQ(e0.slice().getStringLength(), 2048);
  EXPECT_EQ(e0.memoryUsage(), 0);  // This should be zero
  // memory for resourceMonitor should not change
  EXPECT_EQ(rm.current(), originalSize);

  // doCopy = true -> Creates copy, mustDestroy changes to true
  // This creates a new copy of SupervisedSlice AqlValue
  AqlValue e1 = a.at(2, mustDestroy, /*doCopy*/ true);
  EXPECT_TRUE(mustDestroy);
  EXPECT_EQ(e1.slice().getStringLength(), 2048);
  EXPECT_GE(e1.memoryUsage(), ptrOverhead() + 2048);
  EXPECT_EQ(rm.current(), originalSize + e1.memoryUsage());

  e1.destroy();  // This destroy should affect memory usage of ResourceMonitor
  EXPECT_EQ(rm.current(), originalSize);

  a.destroy();
  EXPECT_EQ(rm.current(), 0);
}

// Test behavior of getKeyAttribute() function
// If copy = true, it creates a new copy of heap data
TEST(AqlValueSupervisedTest, FuncGetKeyAttributeWithDoCopyTrueReturnsCopy) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  // =====================================
  // Case 1: Supervised slice WITH a _key
  // =====================================
  Builder b;
  b.openObject();
  {
    std::string bigKey(10000, 'k');
    b.add("_key", Value(bigKey));
  }
  b.close();
  Slice s = b.slice();

  AqlValue v(s, 0, &rm);  // SupervisedSlice
  EXPECT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_TRUE(v.slice().isObject());
  size_t base = rm.current();
  EXPECT_EQ(v.memoryUsage(), base);

  // ---- doCopy = false: returns a reference (slice-pointer)
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

  // ---- doCopy = true: returns a supervised copy; caller must destroy it
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

    // Increase memory since we create a new copy of heap obj
    EXPECT_EQ(rm.current(), base + out.memoryUsage());
    out.destroy();
    EXPECT_EQ(rm.current(), base);
  }

  v.destroy();
  EXPECT_EQ(rm.current(), 0U);

  // ======================================
  // Case 2: Supervised slice WITHOUT _key
  // ======================================
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

  AqlValue v2(s2, 0, &rm);
  EXPECT_TRUE(v2.slice().isObject());
  base = rm.current();
  EXPECT_EQ(v2.memoryUsage(), base);

  // ---- doCopy = false: should return Null (no key)
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

  // ---- doCopy = true: still no key -> Null
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

// Test behavior of getIdAttribute() function
// If doCopy = true, it should return a new copy of heap data -> increase memory
TEST(AqlValueSupervisedTest, FuncGetIdAttributeWithDoCopyTrueReturnsCopy) {
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase = server.getSystemDatabase();
  arangodb::CollectionNameResolver resolver(vocbase);

  auto& global = arangodb::GlobalResourceMonitor::instance();
  arangodb::ResourceMonitor rm(global);
  ASSERT_EQ(rm.current(), 0U);

  // ============================================================
  // Case 1: Large supervised object WITH "_id" (as a normal string)
  // ============================================================
  Builder b1;
  b1.openObject();
  b1.add("_id", Value("mycol/abcdefhijklmn123456789"));
  b1.close();
  Slice s1 = b1.slice();

  AqlValue v1(s1, 0, &rm);  // SupervisedSlice
  EXPECT_EQ(v1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  size_t base1 = rm.current();
  EXPECT_EQ(base1, v1.memoryUsage());

  // --- doCopy = false: should return SlicePointer (reference)
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

  // --- doCopy = true: should produce a copy (supervised)
  {
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue out = v1.getIdAttribute(resolver, mustDestroy, doCopy);
    EXPECT_EQ(out.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(mustDestroy);
    EXPECT_TRUE(out.slice().isString());
    EXPECT_TRUE(out.slice().binaryEquals(s1.get("_id")));

    EXPECT_EQ(rm.current(),
              base1 + out.memoryUsage());  // additional supervised allocation
    out.destroy();                         // releases the supervised copy
    EXPECT_EQ(rm.current(), base1);
  }

  // ============================================================
  // Case 2: Large supervised object WITHOUT "_id"
  // ============================================================
  Builder b2;
  b2.openObject();
  b2.add("big", arangodb::velocypack::Value(std::string(16000, 'y')));
  b2.close();
  Slice s2 = b2.slice();
  ASSERT_TRUE(s2.get("_id").isNone());

  AqlValue v2(s2, 0, &rm);
  EXPECT_EQ(v2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  size_t base2 = rm.current();
  EXPECT_EQ(base2, base1 + v2.memoryUsage());  // v1 + v2 accounted

  // --- doCopy = false: no _id -> returns Null, no destruction, no memory
  // change
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

  // --- doCopy = true: still no _id -> Null, no destruction, no memory change
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

// Test behavior of getFromAttribute() and getToAttribute() functions
// If doCopy = true, it should return a new copy of heap data -> increase memory
TEST(AqlValueSupervisedTest, FuncGetFromAndToAttributesReturnCopy) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  const std::string big(4096, 'x');
  const std::string fromValue = "vertices/users/" + big;
  const std::string toValue = "vertices/posts/12345-" + big;

  // ===========================================================================
  // Document WITH both _from and _to
  // ===========================================================================
  Builder bEdge;
  bEdge.openObject();
  bEdge.add("_from", Value(fromValue));
  bEdge.add("_to", Value(toValue));
  bEdge.add("note", Value(big));  // keep object large
  bEdge.close();

  AqlValue src(Slice(bEdge.slice()), /*byteSize*/ 0, &rm);
  ASSERT_EQ(src.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  auto base = rm.current();

  // ---- getFromAttribute: doCopy = false (reference) ----
  {
    bool mustDestroy = false;
    AqlValue out = src.getFromAttribute(mustDestroy, /*doCopy*/ false);

    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.isString());
    EXPECT_EQ(out.slice().copyString(), fromValue);
    EXPECT_EQ(rm.current(), base);  // no additional memory
  }

  // ---- getFromAttribute: doCopy = true (owning copy) ----
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

  // ---- getToAttribute: doCopy = false (reference) ----
  {
    bool mustDestroy = false;
    AqlValue out = src.getToAttribute(mustDestroy, /*doCopy*/ false);

    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(out.isString());
    EXPECT_EQ(out.slice().copyString(), toValue);
    EXPECT_EQ(rm.current(), base);  // no additional memory
  }

  // ---- getToAttribute: doCopy = true (owning copy) ----
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

  // ===========================================================================
  // Document WITHOUT _from and WITHOUT _to (negative cases)
  // ===========================================================================
  Builder bNoAttrs;
  bNoAttrs.openObject();
  bNoAttrs.add("foo", Value(1));
  bNoAttrs.add("bar", Value(big));  // still large, but no _from/_to
  bNoAttrs.close();

  AqlValue srcNoAttrs(Slice(bNoAttrs.slice()), /*byteSize*/ 0, &rm);
  ASSERT_EQ(srcNoAttrs.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  base = rm.current();

  // ---- getFromAttribute: missing -> null, no ownership ----
  {
    bool mustDestroy = false;
    AqlValue out = srcNoAttrs.getFromAttribute(mustDestroy, /*doCopy*/ true);

    EXPECT_FALSE(mustDestroy);
    EXPECT_EQ(out.getTypeString(), "null");
    EXPECT_EQ(rm.current(), base);  // no memory increase
  }

  // ---- getToAttribute: missing -> null, no ownership ----
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

  // ---------------------------
  // 1) get(by name), doCopy = false  (reference to existing slice)
  // ---------------------------
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

  // ---------------------------
  // 2) get(by name), doCopy = true  (owned clone; mustDestroy = true)
  // ---------------------------
  {
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue got = v.get(resolver, "payload", mustDestroy, doCopy);
    EXPECT_EQ(got.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(mustDestroy);
    ASSERT_FALSE(got.isNone());
    ASSERT_TRUE(got.slice().isString());

    // Memory should have increased for the cloned AqlValue
    EXPECT_EQ(rm.current(), baseMem + got.memoryUsage());
    got.destroy();
    // After destroying the cloned AqlValue, memory returns to base
    EXPECT_EQ(rm.current(), baseMem);
  }

  // ---------------------------
  // 3) get(by list), doCopy = false: ["user", "profile", "name"]
  // ---------------------------
  {
    std::vector<std::string> path = {"user", "profile", "name"};
    bool mustDestroy = false;
    bool doCopy = false;
    AqlValue got = v.get(resolver, path, mustDestroy, doCopy);
    EXPECT_EQ(got.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_FALSE(mustDestroy);
    ASSERT_FALSE(got.isNone());
    ASSERT_TRUE(got.slice().isString());

    // Reference path: no new memory charged
    EXPECT_EQ(rm.current(), baseMem);
    got.destroy();  // no-op
    EXPECT_EQ(rm.current(), baseMem);
  }

  // ---------------------------
  // 4) get(by list), doCopy = true: ["user", "profile", "name"]
  // ---------------------------
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

  // ---------------------------
  // 5) get(by name) missing field -> Null
  // ---------------------------
  {
    bool mustDestroy = false;
    bool doCopy = true;
    AqlValue got = v.get(resolver, "does_not_exist", mustDestroy, doCopy);
    EXPECT_FALSE(mustDestroy);
    EXPECT_TRUE(got.slice().isNull());
    // No extra memory charged
    EXPECT_EQ(rm.current(), baseMem);
    got.destroy();  // safe
    EXPECT_EQ(rm.current(), baseMem);
  }

  // ---------------------------
  // 6) get(by path) missing mid-path -> Null
  // ---------------------------
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

// Test behavior of toDouble() function
// For SupervisedSlice, we need to create string and array object
TEST(AqlValueSupervisedTest, FuncToDoubleReturnsCorrectValue) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  // ---- SupervisedSlice for STRING ----
  {
    std::string big = "123.5";
    big.append(8192, ' ');
    Builder b;
    b.add(Value(big));
    auto s = b.slice();

    AqlValue v(s, s.byteSize(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    bool failed = true;
    double d = v.toDouble(failed);
    EXPECT_FALSE(failed);
    EXPECT_DOUBLE_EQ(d, 123.5);

    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }

  // ---- SupervisedSlice for ARRAY ----
  {
    Builder b;
    b.openArray();
    for (int i = 0; i < 3000; ++i) {
      b.add(arangodb::velocypack::Value(i));
    }
    b.close();

    auto s = b.slice();
    AqlValue v(s, s.byteSize(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    {
      bool failed = true;
      double d = v.toDouble(failed);
      EXPECT_TRUE(failed);
      EXPECT_DOUBLE_EQ(d, 0.0);
    }

    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }
}

// Test behavior of toInt64() function
// For SupervisedSlice, we need to create string and array object
TEST(AqlValueSupervisedTest, FuncToInt64ReturnsCorrectValue) {
  // 1) SupservisedDSlice for STRING (numeric + whitespace) -> parses to int64
  {
    auto& global = GlobalResourceMonitor::instance();
    ResourceMonitor rm(global);
    ASSERT_EQ(rm.current(), 0U);

    std::string big = "12345";
    big.append(8192, ' ');  // whitespace padding

    Builder b;
    b.add(Value(big));
    auto s = b.slice();

    AqlValue v(s, s.byteSize(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    int64_t x = v.toInt64();
    EXPECT_EQ(x, 12345);

    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }

  // 2) SupervisedSlice for ARRAY (single element numeric-as-string) -> element
  // toInt64
  {
    auto& global = GlobalResourceMonitor::instance();
    ResourceMonitor rm(global);
    ASSERT_EQ(rm.current(), 0U);

    // Make a single-element array; the sole element is a LARGE numeric string
    // so the overall slice is big enough to be supervised.
    std::string num = "777";
    num.append(8192, ' ');  // whitespace padding

    arangodb::velocypack::Builder b;
    b.openArray();
    b.add(arangodb::velocypack::Value(num));
    b.close();

    auto s = b.slice();
    AqlValue v(s, s.byteSize(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    // toInt64(): array with length==1 -> at(0).toInt64()
    int64_t y = v.toInt64();
    EXPECT_EQ(y, 777);

    v.destroy();
    EXPECT_EQ(rm.current(), 0U);
  }

  // --- 3) INLINE UINT64 that overflows int64_t -> must throw VPackException
  {
    // Create an inline-uint64 value > INT64_MAX
    uint64_t over =
        static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ULL;
    AqlValue v(AqlValueHintUInt{over});  // produces VPACK_INLINE_UINT64

    EXPECT_THROW({ (void)v.toInt64(); }, VPackException);
    // No destroy() required for inline values.
  }
}

// Test behavior of toBoolean() function
// For SupervisedSlice, we need to create string and array object
TEST(AqlValueSupervisedTest, FuncToBooleanReturnsCorrectValue) {
  auto& global = arangodb::GlobalResourceMonitor::instance();
  arangodb::ResourceMonitor rm(global);

  // --- SupervisedSlice STRING
  {
    std::string big(5000, 'a');
    Builder b;
    b.add(Value(big));
    auto s = b.slice();

    AqlValue v(s, s.byteSize(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(v.toBoolean());  // non-empty string -> true
    v.destroy();
  }

  // --- Supervised ARRAY (large -> true)
  {
    Builder b;
    b.openArray();
    for (int i = 0; i < 1000; ++i) {
      b.add(Value(i));
    }
    b.close();
    auto s = b.slice();

    AqlValue v(s, s.byteSize(), &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);
    EXPECT_TRUE(v.toBoolean());  // arrays -> true per implementation
    v.destroy();
  }
}

// Test behavior of data() function
// data() should return pointer to actual heap data (not resourceMonitor*)
TEST(AqlValueSupervisedTest, FuncDataReturnsPointerToActualData) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  // 1) SupervisedSlice
  Builder b1 = makeString(300, 'a');
  Slice s1 = b1.slice();

  AqlValue a1(s1, 0, &rm);  // supervised slice
  auto e1 = s1.byteSize() + ptrOverhead();
  EXPECT_EQ(a1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  EXPECT_EQ(a1.memoryUsage(), e1);
  EXPECT_EQ(a1.memoryUsage(), rm.current());

  // data() must point to the beginning of this value's slice payload
  // Not points to resourceMonitor*
  EXPECT_EQ(static_cast<uint8_t const*>(a1.data()), a1.slice().start());

  a1.destroy();
  EXPECT_EQ(rm.current(), 0u);
}

// Test behavior of toVelocyPack() function
// This function returns Builder object that represents the heap data
// Need to instantiate AqlValues using various ctors!!!!!
TEST(AqlValueSupervisedTest, FuncToVelocyPackReturnsCorrectBuilder) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor rm(global);

  const Options* opts = &Options::Defaults;
  const std::string big(4096, 'x');

  // STRING
  {
    Builder bin;
    bin.add(Value(std::string("hello-") + big));
    AqlValue v(Slice(bin.slice()), /*byteSize*/ 0, &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    Builder bout;
    v.toVelocyPack(opts, bout, /*allowUnindexed*/ true);

    // Compare JSON representations for equality
    EXPECT_EQ(Slice(bin.slice()).toJson(opts),
              Slice(bout.slice()).toJson(opts));

    v.destroy();
  }

  // OBJECT
  {
    Builder bin;
    bin.openObject();
    bin.add("k1", Value(1));
    bin.add("k2", Value(2));
    bin.add("pad", Value(big));
    bin.add("msg", Value("ok"));
    bin.close();

    AqlValue v(Slice(bin.slice()), 0, &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    Builder bout;
    v.toVelocyPack(opts, bout, /*allowUnindexed*/ false);

    EXPECT_EQ(Slice(bin.slice()).toJson(opts),
              Slice(bout.slice()).toJson(opts));

    v.destroy();
  }

  // ARRAY
  {
    Builder bin;
    bin.openArray();
    bin.add(Value("a"));
    bin.add(Value("b"));
    bin.add(Value(big));  // make array large
    bin.add(Value("c"));
    bin.close();

    AqlValue v(Slice(bin.slice()), 0, &rm);
    ASSERT_EQ(v.type(), AqlValue::VPACK_SUPERVISED_SLICE);

    Builder bout;
    v.toVelocyPack(opts, bout, /*allowUnindexed*/ true);

    EXPECT_EQ(Slice(bin.slice()).toJson(opts),
              Slice(bout.slice()).toJson(opts));

    v.destroy();
  }

  EXPECT_EQ(rm.current(), 0U);
}

// Test the behavior of materialize() function's default case
TEST(AqlValueSupervisedTest,
     FuncMaterializeForSupervisedAqlValueReturnsShallowCopy) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  /* Non-range SupervisedSlice */
  std::string big(8 * 1024, 'x');
  Builder obj;
  obj.openObject();
  obj.add("q", Value(7));
  obj.add("big", Value(big));
  obj.close();

  AqlValue v1(obj.slice(), /*length*/ 0, &rm);  // SupervisedSlice
  std::uint64_t base = rm.current();

  bool copied1 = true;
  AqlValue mat1 = v1.materialize(nullptr, copied1);
  // materialize()'s default case calls copy ctor of SupervisedSlice
  // Copy ctor of SupervisedSlice is defaulted i.e. shallow copy (no new copy of
  // heap data)
  EXPECT_FALSE(copied1);  // This should be true

  EXPECT_TRUE(v1.slice().binaryEquals(obj.slice()));
  EXPECT_TRUE(mat1.slice().binaryEquals(obj.slice()));

  // Because this is shallow copy, no increase memory
  EXPECT_EQ(rm.current(), base);

  mat1.destroy();
  EXPECT_EQ(rm.current(), 0U);

  // We can't call v1.destroy() because v1's dataPointer is dangling
  v1.erase();
  EXPECT_EQ(rm.current(), 0U);
}

// Test if slice() returns slice of actual data
TEST(AqlValueSupervisedTest,
     FuncSliceWithAqlValueTypeReturnsSliceOfActualData) {
  auto& g = GlobalResourceMonitor::instance();
  ResourceMonitor rm(g);

  /* SUPERVISED_SLICE */
  Builder b1 = makeString(300, 'a');
  Slice s1 = b1.slice();
  AqlValue a1(s1, 0, &rm);  // supervised slice
  EXPECT_EQ(a1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  {
    auto sl = a1.slice();
    EXPECT_EQ(sl.getStringLength(), 300);
    // Must point at the actual payload
    EXPECT_EQ(sl.start(), static_cast<uint8_t const*>(a1.data()));
  }

  a1.destroy();
  EXPECT_EQ(rm.current(), 0u);
}

// Test the behavior of clone(); for supervised AqlValue, it should create
// a new copy of heap obj, hence counts double memory usage
TEST(AqlValueSupervisedTest, FuncCloneCreatesAnotherCopy) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  Builder builder;
  builder.openArray();
  builder.add(Value(std::string(1024, 'a')));
  builder.close();
  Slice slice = builder.slice();

  AqlValue aqlVal(slice, 0, &resourceMonitor);  // SUPERVISED_SLICE
  AqlValue cloneVal = aqlVal.clone();           // Create new copy of heap data
  // Those AqlValues are independent, so memory usage should be doubled
  EXPECT_EQ(resourceMonitor.current(),
            aqlVal.memoryUsage() + cloneVal.memoryUsage());

  aqlVal.destroy();
  // cloneVal is still alive
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
  // memoryUsage() should match what the RM accounted
  EXPECT_EQ(mem1, rm.current());
  // and be strictly larger than the raw VPack payload (prefix included)
  EXPECT_EQ(mem1, static_cast<size_t>(s1.byteSize()) + ptrOverhead());

  // Now construct a second, larger supervised slice to check scaling
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
  EXPECT_EQ(diffMem, diffPayload);  // Difference should be the same

  v2.destroy();
  EXPECT_EQ(rm.current(), mem1);
  v1.destroy();
  EXPECT_EQ(rm.current(), 0U);
}

// Test the behavior of duplicate destroy()
TEST(AqlValueSupervisedTest, DuplicateDestroysAreSafe) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor resourceMonitor(global);

  auto b = makeLargeArray(2000, 'a');
  Slice slice = b.slice();
  AqlValue aqlVal(slice, 0, &resourceMonitor);
  auto expected = slice.byteSize() + ptrOverhead();
  EXPECT_EQ(aqlVal.memoryUsage(), expected);
  EXPECT_EQ(aqlVal.memoryUsage(), resourceMonitor.current());

  aqlVal.destroy();
  EXPECT_EQ(resourceMonitor.current(), 0);

  // destroy() calls erase() where zero outs the AqlValue's 16 bytes
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
  AqlValue supervisedA1(sa, 0, &rm);
  ASSERT_EQ(supervisedA1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedA2(sa, 0, &rm);
  ASSERT_EQ(supervisedA2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedB1(sb, 0, &rm);
  ASSERT_EQ(supervisedB1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervisedB2(sb, 0, &rm);
  ASSERT_EQ(supervisedB2.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  // These are the same
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA1, supervisedA1, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA1, supervisedA2, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA2, supervisedA1, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, managedA2, supervisedA2, true), 0);
  EXPECT_EQ(AqlValue::Compare(nullptr, supervisedA1, supervisedA2, true), 0);

  // These are different
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