////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Futures/Try.h"

#include "gtest/gtest.h"

using namespace arangodb::futures;

// from folly Utilities.h
#if __cpp_lib_as_const || _MSC_VER
/* using override */ using std::as_const;
#else
template <class T>
constexpr T const& as_const(T& t) noexcept {
  return t;
}

template <class T>
void as_const(T const&&) = delete;
#endif

namespace {

class A {
 public:
  explicit A(int x) : x_(x) {}

  int x() const { return x_; }

 private:
  int x_;
};

template <bool Nothrow>
class HasCtors {
 public:
  explicit HasCtors(int) noexcept(Nothrow) {}
  HasCtors(HasCtors&&) noexcept(Nothrow) {}
  HasCtors& operator=(HasCtors&&) noexcept(Nothrow) {}
  HasCtors(HasCtors const&) noexcept(Nothrow) {}
  HasCtors& operator=(HasCtors const&) noexcept(Nothrow) {}
};

class MoveConstructOnly {
 public:
  MoveConstructOnly() = default;
  MoveConstructOnly(const MoveConstructOnly&) = delete;
  MoveConstructOnly(MoveConstructOnly&&) = default;
};

class MutableContainer {
 public:
  mutable MoveConstructOnly val;
};
}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST(FuturesTryTest, Basic) {
  A a(5);
  Try<A> t_a(std::move(a));

  // Try<Unit> t_void;

  ASSERT_TRUE(5 == t_a.get().x());
}

TEST(FuturesTryTest, in_place) {
  Try<A> t_a(std::in_place, 5);

  ASSERT_TRUE(5 == t_a.get().x());
}

TEST(FuturesTryTest, in_place_nested) {
  Try<Try<A>> t_t_a(std::in_place, std::in_place, 5);

  ASSERT_TRUE(5 == t_t_a.get().get().x());
}

TEST(FuturesTryTest, assignment_with_throwing_ctor) {
  struct MyException : std::exception {};
  struct ThrowingCopyConstructor {
    int& counter_;
    explicit ThrowingCopyConstructor(int& counter) : counter_(counter) {
      ++counter_;
    }

    [[noreturn]] ThrowingCopyConstructor(const ThrowingCopyConstructor& other) noexcept(false)
        : counter_(other.counter_) {
      throw MyException{};
    }

    ThrowingCopyConstructor& operator=(const ThrowingCopyConstructor&) = delete;

    ~ThrowingCopyConstructor() { --counter_; }
  };

  int counter = 0;

  {
    Try<ThrowingCopyConstructor> t1{std::in_place, counter};
    Try<ThrowingCopyConstructor> t2{std::in_place, counter};
    ASSERT_TRUE(2 == counter);
    EXPECT_ANY_THROW(t2 = t1);
    EXPECT_ANY_THROW(t2 = t1);
    ASSERT_TRUE(1 == counter);
    ASSERT_FALSE(t2.hasValue());
    ASSERT_TRUE(t1.hasValue());
  }
  ASSERT_TRUE(0 == counter);
  {
    Try<ThrowingCopyConstructor> t1{std::in_place, counter};
    Try<ThrowingCopyConstructor> t2;
    ASSERT_TRUE(1 == counter);
    EXPECT_ANY_THROW(t2 = t1);
    ASSERT_TRUE(1 == counter);
    ASSERT_FALSE(t2.hasValue());
    ASSERT_TRUE(t1.hasValue());
  }
  ASSERT_TRUE(0 == counter);
}

// TODO assignmentWithThrowingMoveConstructor

TEST(FuturesTryTest, emplace) {
  Try<A> t;
  A& t_a = t.emplace(10);
  ASSERT_TRUE(t.hasValue());
  ASSERT_TRUE(t_a.x() == 10);
}

TEST(FuturesTryTest, emplace_void) {
  struct MyException : std::exception {};
  Try<void> t;
  t.emplace();
  ASSERT_TRUE(t.hasValue());
  t.set_exception(MyException());
  ASSERT_FALSE(t.hasValue());
  ASSERT_TRUE(t.hasException());
  t.emplace();
  ASSERT_TRUE(t.hasValue());
  ASSERT_FALSE(t.hasException());
}

TEST(FuturesTryTest, MoveConstRvalue) {
  // tests to see if Try returns a const Rvalue, this is required in the case
  // where for example MutableContainer has a mutable member that is move only
  // and you want to fetch the value from the Try and move it into a member
  {
    const Try<MutableContainer> t{std::in_place};
    auto val = MoveConstructOnly(std::move(t).get().val);
    static_cast<void>(val);
  }
  {
    const Try<MutableContainer> t{std::in_place};
    auto val = (*(std::move(t))).val;
    static_cast<void>(val);
  }
}

// Make sure we can copy Trys for copyable types
TEST(FuturesTryTest, copy) {
  Try<int> t;
  auto t2 = t;
}

// But don't choke on move-only types
TEST(FuturesTryTest, moveOnly) {
  Try<std::unique_ptr<int>> t;
  std::vector<Try<std::unique_ptr<int>>> v;
  v.reserve(10);
}

TEST(FuturesTryTest, exception) {
  using ML = std::exception_ptr&;
  using MR = std::exception_ptr&&;
  using CL = std::exception_ptr const&;
  using CR = std::exception_ptr const&&;

  {
    auto obj = Try<int>();
    using ActualML = decltype(obj.exception());
    using ActualMR = decltype(std::move(obj).exception());
    using ActualCL = decltype(as_const(obj).exception());
    using ActualCR = decltype(std::move(as_const(obj)).exception());
    ASSERT_TRUE((std::is_same<ML, ActualML>::value));
    ASSERT_TRUE((std::is_same<MR, ActualMR>::value));
    ASSERT_TRUE((std::is_same<CL, ActualCL>::value));
    ASSERT_TRUE((std::is_same<CR, ActualCR>::value));
  }

  {
    auto obj = Try<int>(3);
    EXPECT_ANY_THROW(obj.exception());
    EXPECT_ANY_THROW(std::move(obj).exception());
    EXPECT_ANY_THROW(as_const(obj).exception());
    EXPECT_ANY_THROW(std::move(as_const(obj)).exception());
  }

  {
    auto obj = Try<int>(std::make_exception_ptr<int>(-3));
    bool didThrow = false;
    try {
      obj.throwIfFailed();
    } catch (int x) {
      didThrow = true;
      ASSERT_TRUE(x == -3);
    } catch (...) {
      FAIL() << "invalid exception type";
    }
    ASSERT_TRUE(didThrow);

    /*EXPECT_EQ(-3, *obj.exception().get_exception<int>());
    EXPECT_EQ(-3, *std::move(obj).exception().get_exception<int>());
    EXPECT_EQ(-3, *as_const(obj).exception().get_exception<int>());
    EXPECT_EQ(-3, *std::move(as_const(obj)).exception().get_exception<int>());*/
  }

  {
    auto obj = Try<void>();
    using ActualML = decltype(obj.exception());
    using ActualMR = decltype(std::move(obj).exception());
    using ActualCL = decltype(as_const(obj).exception());
    using ActualCR = decltype(std::move(as_const(obj)).exception());
    ASSERT_TRUE((std::is_same<ML, ActualML>::value));
    ASSERT_TRUE((std::is_same<MR, ActualMR>::value));
    ASSERT_TRUE((std::is_same<CL, ActualCL>::value));
    ASSERT_TRUE((std::is_same<CR, ActualCR>::value));
  }

  {
    auto obj = Try<void>();
    EXPECT_ANY_THROW(obj.exception());
    EXPECT_ANY_THROW(std::move(obj).exception());
    EXPECT_ANY_THROW(as_const(obj).exception());
    EXPECT_ANY_THROW(std::move(as_const(obj)).exception());
  }

  {
    auto obj = Try<void>(std::make_exception_ptr<int>(-3));
    bool didThrow = false;
    try {
      obj.throwIfFailed();
    } catch (int x) {
      didThrow = true;
      ASSERT_TRUE(x == -3);
    } catch (...) {
      FAIL() << "invalid exception type";
    }
    ASSERT_TRUE(didThrow);
    /*EXPECT_EQ(-3, *obj.exception().get_exception<int>());
    EXPECT_EQ(-3, *std::move(obj).exception().get_exception<int>());
    EXPECT_EQ(-3, *as_const(obj).exception().get_exception<int>());
    EXPECT_EQ(-3, *std::move(as_const(obj)).exception().get_exception<int>());*/
  }
}
