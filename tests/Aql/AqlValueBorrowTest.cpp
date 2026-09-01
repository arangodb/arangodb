////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
#include "Aql/Range.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include <string>

using namespace arangodb::aql;

namespace {

// larger than the 15 byte inline budget, so it becomes VPACK_MANAGED_SLICE
std::shared_ptr<arangodb::velocypack::Builder> largeObject() {
  return arangodb::velocypack::Parser::fromJson(
      "{\"someLongAttributeName\":\"someLongAttributeValue\"}");
}

}  // namespace

// Ownership is a pure function of the type byte. borrow() hands out something
// that owns nothing, so destroying it must never touch the owner's payload.
// These run under ASAN in CI, so a double free or use after free fails loudly.

TEST(AqlValueBorrowTest, ownership_follows_the_type_byte) {
  auto builder = largeObject();
  AqlValue managed{builder->slice()};
  AqlValue range{int64_t{1}, int64_t{10}};
  AqlValue pointer{AqlValueHintSliceNoCopy{builder->slice()}};
  AqlValue inlineValue{AqlValueHintInt{42}};

  EXPECT_TRUE(managed.requiresDestruction());
  EXPECT_TRUE(range.requiresDestruction());
  EXPECT_FALSE(pointer.requiresDestruction());
  EXPECT_FALSE(inlineValue.requiresDestruction());

  managed.destroy();
  range.destroy();
}

TEST(AqlValueBorrowTest, managed_slice_borrow_becomes_a_short_lived_pointer) {
  auto builder = largeObject();
  AqlValue owner{builder->slice()};
  ASSERT_EQ(owner.type(), AqlValue::VPACK_MANAGED_SLICE);

  {
    AqlValue borrowed = owner.borrow();
    EXPECT_EQ(borrowed.type(), AqlValue::VPACK_SLICE_POINTER);
    EXPECT_TRUE(borrowed.isShortLivedSlice());
    EXPECT_FALSE(borrowed.isStaticSlice());
    EXPECT_FALSE(borrowed.requiresDestruction());
    EXPECT_EQ(borrowed.slice().begin(), owner.slice().begin());
    // must be a no-op, the owner still needs the payload afterwards
    borrowed.destroy();
  }

  EXPECT_EQ(owner.slice().get("someLongAttributeName").stringView(),
            "someLongAttributeValue");
  owner.destroy();
}

TEST(AqlValueBorrowTest, managed_string_borrow_points_at_the_vpack) {
  AqlValue owner{std::string_view{
      "a string well beyond the inline budget of fifteen bytes"}};
  ASSERT_TRUE(owner.requiresDestruction());

  AqlValue borrowed = owner.borrow();
  EXPECT_EQ(borrowed.type(), AqlValue::VPACK_SLICE_POINTER);
  EXPECT_FALSE(borrowed.requiresDestruction());
  EXPECT_EQ(borrowed.slice().begin(), owner.slice().begin());
  EXPECT_TRUE(borrowed.slice().isString());
  borrowed.destroy();

  EXPECT_TRUE(owner.slice().isString());
  owner.destroy();
}

TEST(AqlValueBorrowTest, range_borrow_copies) {
  AqlValue owner{int64_t{1}, int64_t{10}};
  ASSERT_EQ(owner.type(), AqlValue::RANGE);

  AqlValue copy = owner.borrow();
  // a range has no pointer form, so it is treated as a value
  EXPECT_EQ(copy.type(), AqlValue::RANGE);
  EXPECT_TRUE(copy.requiresDestruction());
  EXPECT_NE(copy.range(), owner.range());
  EXPECT_EQ(copy.range()->_low, owner.range()->_low);
  EXPECT_EQ(copy.range()->_high, owner.range()->_high);

  copy.destroy();
  EXPECT_EQ(owner.range()->_low, 1);
  EXPECT_EQ(owner.range()->_high, 10);
  owner.destroy();
}

TEST(AqlValueBorrowTest, inline_borrow_is_a_plain_copy) {
  AqlValue const values[] = {
      AqlValue{AqlValueHintInt{42}}, AqlValue{AqlValueHintUInt{42}},
      AqlValue{AqlValueHintDouble{4.2}}, AqlValue{AqlValueHintBool{true}},
      AqlValue{AqlValueHintNull{}}};

  for (auto const& v : values) {
    AqlValue b = v.borrow();
    EXPECT_EQ(b.type(), v.type());
    EXPECT_FALSE(b.requiresDestruction());
    // an inline value points at nothing, so it is neither static nor
    // short-lived
    EXPECT_FALSE(b.isStaticSlice());
    EXPECT_FALSE(b.isShortLivedSlice());
  }
}

TEST(AqlValueBorrowTest, slice_pointers_default_to_short_lived) {
  auto builder = largeObject();
  AqlValue v{AqlValueHintSliceNoCopy{builder->slice()}};
  ASSERT_EQ(v.type(), AqlValue::VPACK_SLICE_POINTER);
  EXPECT_TRUE(v.isShortLivedSlice());
  EXPECT_FALSE(v.isStaticSlice());
}

TEST(AqlValueBorrowTest, static_slices_are_marked) {
  auto builder = largeObject();
  AqlValue v = AqlValue::staticSlice(builder->slice().begin());
  ASSERT_EQ(v.type(), AqlValue::VPACK_SLICE_POINTER);
  EXPECT_TRUE(v.isStaticSlice());
  EXPECT_FALSE(v.isShortLivedSlice());
  EXPECT_FALSE(v.requiresDestruction());
  EXPECT_EQ(v.slice().begin(), builder->slice().begin());
  EXPECT_TRUE(v.slice().isObject());

  // the marker must survive being handed around
  AqlValue again = v.borrow();
  EXPECT_TRUE(again.isStaticSlice());
}

TEST(AqlValueBorrowTest, erase_clears_the_static_marker) {
  auto builder = largeObject();
  AqlValue v = AqlValue::staticSlice(builder->slice().begin());
  ASSERT_TRUE(v.isStaticSlice());

  v.erase();
  EXPECT_TRUE(v.isEmpty());
  EXPECT_FALSE(v.isStaticSlice());
  EXPECT_FALSE(v.isShortLivedSlice());
}

// clone() is not yet uniform: it deep-copies the managed types but bit-copies a
// slice pointer, via the `default: break` in AqlValue::clone(). So cloning a
// borrow currently hands back another pointer to the owner's payload rather
// than an independent value. Pinned here so the gap is visible; making clone()
// uniform is the next step, and needs a check that every query-static producer
// is marked first, or constants start being copied per row.
TEST(AqlValueBorrowTest, clone_of_a_borrow_does_not_yet_detach) {
  auto builder = largeObject();
  AqlValue owner{builder->slice()};
  AqlValue borrowed = owner.borrow();

  AqlValue copy = borrowed.clone();
  EXPECT_FALSE(copy.requiresDestruction());
  EXPECT_EQ(copy.slice().begin(), owner.slice().begin());

  owner.destroy();
}

// the target behaviour, enable together with the clone() change
TEST(AqlValueBorrowTest, DISABLED_clone_of_a_borrow_owns_its_copy) {
  auto builder = largeObject();
  AqlValue owner{builder->slice()};
  AqlValue borrowed = owner.borrow();

  AqlValue copy = borrowed.clone();
  EXPECT_TRUE(copy.requiresDestruction());
  EXPECT_NE(copy.slice().begin(), owner.slice().begin());
  EXPECT_GT(copy.memoryUsage(), 0);

  owner.destroy();
  // the clone must survive the owner
  EXPECT_EQ(copy.slice().get("someLongAttributeName").stringView(),
            "someLongAttributeValue");
  copy.destroy();
}

TEST(AqlValueBorrowTest, memory_origin_is_unaffected) {
  // byte 1 carries MemoryOriginType for a managed slice and the static marker
  // for a slice pointer. the two layouts must not interfere.
  auto builder = largeObject();
  AqlValue owner{builder->slice()};
  AqlValue borrowed = owner.borrow();
  EXPECT_FALSE(borrowed.requiresDestruction());
  EXPECT_GT(owner.memoryUsage(), 0);
  // destroying the owner must still pick the right deallocation path
  owner.destroy();
  EXPECT_TRUE(owner.isEmpty());
}
