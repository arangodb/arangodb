////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Basics/overload.h"

#include "gtest/gtest.h"

#include <variant>

using namespace arangodb;

class OverloadTest : public ::testing::Test {};

TEST_F(OverloadTest, single_overload_no_args_void_return) {
  auto i = int{0};
  auto const call = overload{
      [&]() { ++i; },
  };
  static_assert(std::is_same_v<void, std::invoke_result_t<decltype(call)>>);
  ASSERT_EQ(0, i);
  i = 0;
  call();
  ASSERT_EQ(1, i);
}

TEST_F(OverloadTest, single_overload_no_args_with_return) {
  auto i = int{0};
  auto const call = overload{
      [&]() { return i + 1; },
  };
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(call)>>);
  ASSERT_EQ(0, i);
  i = 0;
  auto result = call();
  EXPECT_EQ(1, result);
  ASSERT_EQ(0, i);
}

TEST_F(OverloadTest, single_overload_one_arg_void_return) {
  auto i = int{0};
  auto const call = overload{
      [](int& i) { ++i; },
  };
  static_assert(std::is_same_v<void, std::invoke_result_t<decltype(call), int&>>);
  ASSERT_EQ(0, i);
  i = 0;
  call(i);
  ASSERT_EQ(1, i);
}

TEST_F(OverloadTest, single_overload_one_arg_with_return) {
  auto i = int{0};
  auto const call = overload{
      [](int i) { return i + 1; },
  };
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(call), int>>);
  ASSERT_EQ(0, i);
  i = 0;
  auto result = call(i);
  EXPECT_EQ(1, result);
  ASSERT_EQ(0, i);
}

TEST_F(OverloadTest, overload_heterogenous_return_type_with_default) {
  struct A {
    int a{};
  };
  struct B {
    int b{};
  };
  struct C {
    int c{};
  };
  struct D {
    int d{};
  };

  auto const call = overload{
      [](A& x) {
        x.a += 1;
        return x;
      },
      [](B& x) {
        x.b += 2;
        return x;
      },
      [](auto& x) { return x; },
  };
  static_assert(std::is_same_v<A, std::invoke_result_t<decltype(call), A&>>);
  static_assert(std::is_same_v<B, std::invoke_result_t<decltype(call), B&>>);
  static_assert(std::is_same_v<C, std::invoke_result_t<decltype(call), C&>>);
  static_assert(std::is_same_v<D, std::invoke_result_t<decltype(call), D&>>);

  {
    auto a = A{1};
    auto result = call(a);
    static_assert(std::is_same_v<A, decltype(result)>);
    EXPECT_EQ(2, a.a);
    EXPECT_EQ(2, result.a);
  }
  {
    auto b = B{1};
    auto result = call(b);
    static_assert(std::is_same_v<B, decltype(result)>);
    EXPECT_EQ(3, b.b);
    EXPECT_EQ(3, result.b);
  }
  {
    auto c = C{1};
    auto result = call(c);
    static_assert(std::is_same_v<C, decltype(result)>);
    EXPECT_EQ(1, c.c);
    EXPECT_EQ(1, result.c);
  }
  {
    auto d = D{1};
    auto result = call(d);
    static_assert(std::is_same_v<D, decltype(result)>);
    EXPECT_EQ(1, d.d);
    EXPECT_EQ(1, result.d);
  }
}


TEST_F(OverloadTest, overload_differing_return_type) {
  auto const call = overload{
      [](int i) { return i + 1; },
      [](double d) { return d / 2; },
  };
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(call), int>>);
  static_assert(std::is_same_v<double, std::invoke_result_t<decltype(call), double>>);
  auto intResult = call(int{1});
  static_assert(std::is_same_v<int, decltype(intResult)>);
  EXPECT_EQ(2, intResult);
  auto doubleResult = call(double{1});
  static_assert(std::is_same_v<double, decltype(doubleResult)>);
  EXPECT_EQ(0.5, doubleResult);
}

TEST_F(OverloadTest, overload_same_return_type) {
  auto const call = overload{
      [](int i) { return static_cast<double>(i + 1); },
      [](double d) { return d / 2; },
  };
  static_assert(std::is_same_v<double, std::invoke_result_t<decltype(call), int>>);
  static_assert(std::is_same_v<double, std::invoke_result_t<decltype(call), double>>);
  auto intResult = call(int{1});
  static_assert(std::is_same_v<double, decltype(intResult)>);
  EXPECT_EQ(2.0, intResult);
  auto doubleResult = call(double{1});
  static_assert(std::is_same_v<double, decltype(doubleResult)>);
  EXPECT_EQ(0.5, doubleResult);
}

TEST_F(OverloadTest, visit_overload_void_return_type) {
  struct A {
    int a{};
  };
  struct B {
    int b{};
  };

  auto const visitor = overload{
      [](A& x) { x.a += 1; },
      [](B& x) { x.b += 2; },
  };
  static_assert(std::is_same_v<void, std::invoke_result_t<decltype(visitor), A&>>);
  static_assert(std::is_same_v<void, std::invoke_result_t<decltype(visitor), B&>>);

  {
    std::variant<A, B> variant = A{1};
    std::visit(visitor, variant);
    ASSERT_TRUE(std::holds_alternative<A>(variant));
    ASSERT_EQ(2, std::get<A>(variant).a);
  }

  {
    std::variant<A, B> variant = B{1};
    std::visit(visitor, variant);
    ASSERT_TRUE(std::holds_alternative<B>(variant));
    ASSERT_EQ(3, std::get<B>(variant).b);
  }
}

TEST_F(OverloadTest, visit_overload_homogenous_return_type) {
  struct A {
    int a{};
  };
  struct B {
    int b{};
  };

  auto const visitor = overload{
      [](A const& x) { return x.a + 1; },
      [](B const& x) { return x.b + 2; },
  };
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(visitor), A>>);
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(visitor), B>>);

  {
    std::variant<A, B> variant = A{1};
    auto resultA = std::visit(visitor, variant);
    static_assert(std::is_same_v<int, decltype(resultA)>);
    EXPECT_EQ(2, resultA);
    ASSERT_TRUE(std::holds_alternative<A>(variant));
    ASSERT_EQ(1, std::get<A>(variant).a);
  }

  {
    std::variant<A, B> variant = B{1};
    auto resultB = std::visit(visitor, variant);
    static_assert(std::is_same_v<int, decltype(resultB)>);
    EXPECT_EQ(3, resultB);
    ASSERT_TRUE(std::holds_alternative<B>(variant));
    ASSERT_EQ(1, std::get<B>(variant).b);
  }
}

TEST_F(OverloadTest, visit_overload_homogenous_return_type_with_default) {
  struct A {
    int a{};
  };
  struct B {
    int b{};
  };
  struct C {
    int c{};
  };
  struct D {
    int d{};
  };

  auto const visitor = overload{
      [](A const& x) { return x.a + 1; },
      [](B const& x) { return x.b + 2; },
      [](auto const& x) { return -1; },
  };
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(visitor), A>>);
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(visitor), B>>);
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(visitor), C>>);
  static_assert(std::is_same_v<int, std::invoke_result_t<decltype(visitor), D>>);

  {
    std::variant<A, B, C, D> variant = A{1};

    auto resultA = std::visit(visitor, variant);
    static_assert(std::is_same_v<int, decltype(resultA)>);
    EXPECT_EQ(2, resultA);

    ASSERT_TRUE(std::holds_alternative<A>(variant));
    ASSERT_EQ(1, std::get<A>(variant).a);
  }

  {
    std::variant<A, B, C, D> variant = B{1};

    std::visit(visitor, variant);

    auto resultB = std::visit(visitor, variant);
    static_assert(std::is_same_v<int, decltype(resultB)>);
    EXPECT_EQ(3, resultB);

    ASSERT_TRUE(std::holds_alternative<B>(variant));
    ASSERT_EQ(1, std::get<B>(variant).b);
  }

  {
    std::variant<A, B, C, D> variant = C{1};

    std::visit(visitor, variant);

    auto resultC = std::visit(visitor, variant);
    static_assert(std::is_same_v<int, decltype(resultC)>);
    EXPECT_EQ(-1, resultC);

    ASSERT_TRUE(std::holds_alternative<C>(variant));
    ASSERT_EQ(1, std::get<C>(variant).c);
  }

  {
    std::variant<A, B, C, D> variant = D{1};

    std::visit(visitor, variant);

    auto resultD = std::visit(visitor, variant);
    static_assert(std::is_same_v<int, decltype(resultD)>);
    EXPECT_EQ(-1, resultD);

    ASSERT_TRUE(std::holds_alternative<D>(variant));
    ASSERT_EQ(1, std::get<D>(variant).d);
  }
}
