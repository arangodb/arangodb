// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/container/internal/hash_function_defaults.h"

#include <functional>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "absl/random/random.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_test_helpers.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {
namespace {

using ::testing::Types;

TEST(Eq, Int32) {
  hash_default_eq<int32_t> eq;
  EXPECT_TRUE(eq(1, 1u));
  EXPECT_TRUE(eq(1, char{1}));
  EXPECT_TRUE(eq(1, true));
  EXPECT_TRUE(eq(1, double{1.1}));
  EXPECT_FALSE(eq(1, char{2}));
  EXPECT_FALSE(eq(1, 2u));
  EXPECT_FALSE(eq(1, false));
  EXPECT_FALSE(eq(1, 2.));
}

TEST(Hash, Int32) {
  hash_default_hash<int32_t> hash;
  auto h = hash(1);
  EXPECT_EQ(h, hash(1u));
  EXPECT_EQ(h, hash(char{1}));
  EXPECT_EQ(h, hash(true));
  EXPECT_EQ(h, hash(double{1.1}));
  EXPECT_NE(h, hash(2u));
  EXPECT_NE(h, hash(char{2}));
  EXPECT_NE(h, hash(false));
  EXPECT_NE(h, hash(2.));
}

enum class MyEnum { A, B, C, D };

TEST(Eq, Enum) {
  hash_default_eq<MyEnum> eq;
  EXPECT_TRUE(eq(MyEnum::A, MyEnum::A));
  EXPECT_FALSE(eq(MyEnum::A, MyEnum::B));
}

TEST(Hash, Enum) {
  hash_default_hash<MyEnum> hash;

  for (MyEnum e : {MyEnum::A, MyEnum::B, MyEnum::C}) {
    auto h = hash(e);
    EXPECT_EQ(h, hash_default_hash<int>{}(static_cast<int>(e)));
    EXPECT_NE(h, hash(MyEnum::D));
  }
}

using StringTypes = ::testing::Types<std::string, absl::string_view>;

template <class T>
struct EqString : ::testing::Test {
  hash_default_eq<T> key_eq;
};

TYPED_TEST_SUITE(EqString, StringTypes);

template <class T>
struct HashString : ::testing::Test {
  hash_default_hash<T> hasher;
};

TYPED_TEST_SUITE(HashString, StringTypes);

TYPED_TEST(EqString, Works) {
  auto eq = this->key_eq;
  EXPECT_TRUE(eq("a", "a"));
  EXPECT_TRUE(eq("a", absl::string_view("a")));
  EXPECT_TRUE(eq("a", std::string("a")));
  EXPECT_FALSE(eq("a", "b"));
  EXPECT_FALSE(eq("a", absl::string_view("b")));
  EXPECT_FALSE(eq("a", std::string("b")));
}

TYPED_TEST(HashString, Works) {
  auto hash = this->hasher;
  auto h = hash("a");
  EXPECT_EQ(h, hash(absl::string_view("a")));
  EXPECT_EQ(h, hash(std::string("a")));
  EXPECT_NE(h, hash(absl::string_view("b")));
  EXPECT_NE(h, hash(std::string("b")));
}

struct NoDeleter {
  template <class T>
  void operator()(const T* ptr) const {}
};

using PointerTypes =
    ::testing::Types<const int*, int*, std::unique_ptr<const int>,
                     std::unique_ptr<const int, NoDeleter>,
                     std::unique_ptr<int>, std::unique_ptr<int, NoDeleter>,
                     std::shared_ptr<const int>, std::shared_ptr<int>>;

template <class T>
struct EqPointer : ::testing::Test {
  hash_default_eq<T> key_eq;
};

TYPED_TEST_SUITE(EqPointer, PointerTypes);

template <class T>
struct HashPointer : ::testing::Test {
  hash_default_hash<T> hasher;
};

TYPED_TEST_SUITE(HashPointer, PointerTypes);

TYPED_TEST(EqPointer, Works) {
  int dummy;
  auto eq = this->key_eq;
  auto sptr = std::make_shared<int>();
  std::shared_ptr<const int> csptr = sptr;
  int* ptr = sptr.get();
  const int* cptr = ptr;
  std::unique_ptr<int, NoDeleter> uptr(ptr);
  std::unique_ptr<const int, NoDeleter> cuptr(ptr);

  EXPECT_TRUE(eq(ptr, cptr));
  EXPECT_TRUE(eq(ptr, sptr));
  EXPECT_TRUE(eq(ptr, uptr));
  EXPECT_TRUE(eq(ptr, csptr));
  EXPECT_TRUE(eq(ptr, cuptr));
  EXPECT_FALSE(eq(&dummy, cptr));
  EXPECT_FALSE(eq(&dummy, sptr));
  EXPECT_FALSE(eq(&dummy, uptr));
  EXPECT_FALSE(eq(&dummy, csptr));
  EXPECT_FALSE(eq(&dummy, cuptr));
}

TEST(Hash, DerivedAndBase) {
  struct Base {};
  struct Derived : Base {};

  hash_default_hash<Base*> hasher;

  Base base;
  Derived derived;
  EXPECT_NE(hasher(&base), hasher(&derived));
  EXPECT_EQ(hasher(static_cast<Base*>(&derived)), hasher(&derived));

  auto dp = std::make_shared<Derived>();
  EXPECT_EQ(hasher(static_cast<Base*>(dp.get())), hasher(dp));
}

TEST(Hash, FunctionPointer) {
  using Func = int (*)();
  hash_default_hash<Func> hasher;
  hash_default_eq<Func> eq;

  Func p1 = [] { return 1; }, p2 = [] { return 2; };
  EXPECT_EQ(hasher(p1), hasher(p1));
  EXPECT_TRUE(eq(p1, p1));

  EXPECT_NE(hasher(p1), hasher(p2));
  EXPECT_FALSE(eq(p1, p2));
}

TYPED_TEST(HashPointer, Works) {
  int dummy;
  auto hash = this->hasher;
  auto sptr = std::make_shared<int>();
  std::shared_ptr<const int> csptr = sptr;
  int* ptr = sptr.get();
  const int* cptr = ptr;
  std::unique_ptr<int, NoDeleter> uptr(ptr);
  std::unique_ptr<const int, NoDeleter> cuptr(ptr);

  EXPECT_EQ(hash(ptr), hash(cptr));
  EXPECT_EQ(hash(ptr), hash(sptr));
  EXPECT_EQ(hash(ptr), hash(uptr));
  EXPECT_EQ(hash(ptr), hash(csptr));
  EXPECT_EQ(hash(ptr), hash(cuptr));
  EXPECT_NE(hash(&dummy), hash(cptr));
  EXPECT_NE(hash(&dummy), hash(sptr));
  EXPECT_NE(hash(&dummy), hash(uptr));
  EXPECT_NE(hash(&dummy), hash(csptr));
  EXPECT_NE(hash(&dummy), hash(cuptr));
}

TEST(EqCord, Works) {
  hash_default_eq<absl::Cord> eq;
  const absl::string_view a_string_view = "a";
  const absl::Cord a_cord(a_string_view);
  const absl::string_view b_string_view = "b";
  const absl::Cord b_cord(b_string_view);

  EXPECT_TRUE(eq(a_cord, a_cord));
  EXPECT_TRUE(eq(a_cord, a_string_view));
  EXPECT_TRUE(eq(a_string_view, a_cord));
  EXPECT_FALSE(eq(a_cord, b_cord));
  EXPECT_FALSE(eq(a_cord, b_string_view));
  EXPECT_FALSE(eq(b_string_view, a_cord));
}

TEST(HashCord, Works) {
  hash_default_hash<absl::Cord> hash;
  const absl::string_view a_string_view = "a";
  const absl::Cord a_cord(a_string_view);
  const absl::string_view b_string_view = "b";
  const absl::Cord b_cord(b_string_view);

  EXPECT_EQ(hash(a_cord), hash(a_cord));
  EXPECT_EQ(hash(b_cord), hash(b_cord));
  EXPECT_EQ(hash(a_string_view), hash(a_cord));
  EXPECT_EQ(hash(b_string_view), hash(b_cord));
  EXPECT_EQ(hash(absl::Cord("")), hash(""));
  EXPECT_EQ(hash(absl::Cord()), hash(absl::string_view()));

  EXPECT_NE(hash(a_cord), hash(b_cord));
  EXPECT_NE(hash(a_cord), hash(b_string_view));
  EXPECT_NE(hash(a_string_view), hash(b_cord));
  EXPECT_NE(hash(a_string_view), hash(b_string_view));
}

void NoOpReleaser(absl::string_view data, void* arg) {}

TEST(HashCord, FragmentedCordWorks) {
  hash_default_hash<absl::Cord> hash;
  absl::Cord c = absl::MakeFragmentedCord({"a", "b", "c"});
  EXPECT_FALSE(c.TryFlat().has_value());
  EXPECT_EQ(hash(c), hash("abc"));
}

TEST(HashCord, FragmentedLongCordWorks) {
  hash_default_hash<absl::Cord> hash;
  // Crete some large strings which do not fit on the stack.
  std::string a(65536, 'a');
  std::string b(65536, 'b');
  absl::Cord c = absl::MakeFragmentedCord({a, b});
  EXPECT_FALSE(c.TryFlat().has_value());
  EXPECT_EQ(hash(c), hash(a + b));
}

TEST(HashCord, RandomCord) {
  hash_default_hash<absl::Cord> hash;
  auto bitgen = absl::BitGen();
  for (int i = 0; i < 1000; ++i) {
    const int number_of_segments = absl::Uniform(bitgen, 0, 10);
    std::vector<std::string> pieces;
    for (size_t s = 0; s < number_of_segments; ++s) {
      std::string str;
      str.resize(absl::Uniform(bitgen, 0, 4096));
      // MSVC needed the explicit return type in the lambda.
      std::generate(str.begin(), str.end(), [&]() -> char {
        return static_cast<char>(absl::Uniform<unsigned char>(bitgen));
      });
      pieces.push_back(str);
    }
    absl::Cord c = absl::MakeFragmentedCord(pieces);
    EXPECT_EQ(hash(c), hash(std::string(c)));
  }
}

// Cartesian product of (std::string, absl::string_view)
// with (std::string, absl::string_view, const char*, absl::Cord).
using StringTypesCartesianProduct = Types<
    // clang-format off
    std::pair<absl::Cord, std::string>,
    std::pair<absl::Cord, absl::string_view>,
    std::pair<absl::Cord, absl::Cord>,
    std::pair<absl::Cord, const char*>,

    std::pair<std::string, absl::Cord>,
    std::pair<absl::string_view, absl::Cord>,

    std::pair<absl::string_view, std::string>,
    std::pair<absl::string_view, absl::string_view>,
    std::pair<absl::string_view, const char*>>;
// clang-format on

constexpr char kFirstString[] = "abc123";
constexpr char kSecondString[] = "ijk456";

template <typename T>
struct StringLikeTest : public ::testing::Test {
  typename T::first_type a1{kFirstString};
  typename T::second_type b1{kFirstString};
  typename T::first_type a2{kSecondString};
  typename T::second_type b2{kSecondString};
  hash_default_eq<typename T::first_type> eq;
  hash_default_hash<typename T::first_type> hash;
};

TYPED_TEST_SUITE_P(StringLikeTest);

TYPED_TEST_P(StringLikeTest, Eq) {
  EXPECT_TRUE(this->eq(this->a1, this->b1));
  EXPECT_TRUE(this->eq(this->b1, this->a1));
}

TYPED_TEST_P(StringLikeTest, NotEq) {
  EXPECT_FALSE(this->eq(this->a1, this->b2));
  EXPECT_FALSE(this->eq(this->b2, this->a1));
}

TYPED_TEST_P(StringLikeTest, HashEq) {
  EXPECT_EQ(this->hash(this->a1), this->hash(this->b1));
  EXPECT_EQ(this->hash(this->a2), this->hash(this->b2));
  // It would be a poor hash function which collides on these strings.
  EXPECT_NE(this->hash(this->a1), this->hash(this->b2));
}

TYPED_TEST_SUITE(StringLikeTest, StringTypesCartesianProduct);

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl

enum Hash : size_t {
  kStd = 0x1,       // std::hash
#ifdef _MSC_VER
  kExtension = kStd,  // In MSVC, std::hash == ::hash
#else                 // _MSC_VER
  kExtension = 0x2,  // ::hash (GCC extension)
#endif                // _MSC_VER
};

// H is a bitmask of Hash enumerations.
// Hashable<H> is hashable via all means specified in H.
template <int H>
struct Hashable {
  static constexpr bool HashableBy(Hash h) { return h & H; }
};

namespace std {
template <int H>
struct hash<Hashable<H>> {
  template <class E = Hashable<H>,
            class = typename std::enable_if<E::HashableBy(kStd)>::type>
  size_t operator()(E) const {
    return kStd;
  }
};
}  // namespace std

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {
namespace {

template <class T>
size_t Hash(const T& v) {
  return hash_default_hash<T>()(v);
}

TEST(Delegate, HashDispatch) {
  EXPECT_EQ(Hash(kStd), Hash(Hashable<kStd>()));
}

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
