#include "test-helper.h"

struct CallTests : testing::Test {};

TEST_F(CallTests, lambda_call_by_lvalue_reference_finally) {
  bool was_called = false;

  auto cb = [&](int x) noexcept { was_called = true; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).finally(cb);
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, lambda_call_by_lvalue_reference_and_then) {
  bool was_called = false;

  auto cb = [&](int x) noexcept { was_called = true; return 5; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).and_then(cb).finally([](int x) noexcept {});
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, lambda_call_by_const_lvalue_reference_finally) {
  bool was_called = false;

  auto const cb = [&](int x) noexcept { was_called = true; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).finally(cb);
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, lambda_call_by_const_lvalue_reference_and_then) {
  bool was_called = false;

  auto const cb = [&](int x) noexcept { was_called = true; return 5; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).and_then(cb).finally([](int x) noexcept {});
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, mutable_lambda_call_by_lvalue_reference_finally) {
  bool was_called = false;

  auto cb = [&, y = 3](int x) mutable noexcept {  y = x; was_called = true; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).finally(cb);
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, mutable_lambda_call_by_lvalue_reference_and_then) {
  bool was_called = false;

  auto cb = [&, y = 3](int x) mutable noexcept { was_called = true; y = x; return 5; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).and_then(cb).finally([](int x) noexcept {});
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, mutable_lambda_call_by_const_reference_finally) {
  /*bool was_called = false;

  auto const cb = [&, y = 3](int x) mutable noexcept {  y = x; was_called = true; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).finally(cb);
  EXPECT_TRUE(was_called);*/
}

TEST_F(CallTests, mutable_lambda_call_by_const_reference_and_then) {
  /*bool was_called = false;

  auto const cb = [&, y = 3](int x) mutable noexcept { was_called = true; y = x; return 5; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).and_then(cb).finally([](int x) noexcept {});
  EXPECT_TRUE(was_called);*/
}


TEST_F(CallTests, lambda_call_by_rvalue_reference_finally) {
  bool was_called = false;

  auto cb = [&](int x) noexcept { was_called = true; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).finally(std::move(cb));
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, lambda_call_by_rvalue_reference_and_then) {
  bool was_called = false;

  auto cb = [&](int x) noexcept { was_called = true; return 5; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).and_then(std::move(cb)).finally([](int x) noexcept {});
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, lambda_call_by_const_rvalue_reference_finally) {
  bool was_called = false;

  auto const cb = [&](int x) noexcept { was_called = true; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).finally(std::move(cb));
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, lambda_call_by_const_rvalue_reference_and_then) {
  bool was_called = false;

  auto const cb = [&](int x) noexcept { was_called = true; return 5; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).and_then(std::move(cb)).finally([](int x) noexcept {});
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, mutable_lambda_call_by_rvalue_reference_finally) {
  bool was_called = false;

  auto cb = [&, y = 3](int x) mutable noexcept {  y = x; was_called = true; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).finally(std::move(cb));
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, mutable_lambda_call_by_rvalue_reference_and_then) {
  bool was_called = false;

  auto cb = [&, y = 3](int x) mutable noexcept { was_called = true; y = x; return 5; };

  auto f = future<int>{std::in_place, 12};
  std::move(f).and_then(std::move(cb)).finally([](int x) noexcept {});
  EXPECT_TRUE(was_called);
}


TEST_F(CallTests, std_function_call_by_lvalue_reference_then) {

  bool was_called = false;
  std::function<int(int)> cb;

  auto f = future<expect::expected<int>>{std::in_place, 12};
  std::move(f).then(cb).finally([&](auto) noexcept { was_called = true; });
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, std_function_call_by_const_lvalue_reference_then) {

  bool was_called = false;
  std::function<int(int)> const cb;

  auto f = future<expect::expected<int>>{std::in_place, 12};
  std::move(f).then(cb).finally([&](auto) noexcept { was_called = true; });
  EXPECT_TRUE(was_called);
}

TEST_F(CallTests, std_function_call_by_rvalue_reference_then) {

  bool was_called = false;
  std::function<int(int)> cb;

  auto f = future<expect::expected<int>>{std::in_place, 12};
  std::move(f).then(std::move(cb)).finally([&](auto) noexcept { was_called = true; });
  EXPECT_TRUE(was_called);
}
