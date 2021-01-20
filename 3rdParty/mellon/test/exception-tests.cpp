#include "test-helper.h"

using namespace expect;


template <typename Tag>
struct ExceptionTests : public testing::Test {
  signal_marker reached_last{"reached-last"};
};

TYPED_TEST_SUITE(ExceptionTests, my_test_tags);

TYPED_TEST(ExceptionTests, promise_fulfill) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f)
      .template catch_error<std::runtime_error>([](auto&& e) {
        ADD_FAILURE() << "this should not be executed";
        return 5;
      })
      .then([](int x) { return 2 * x; })
      .finally([&](expected<int> x) noexcept {
        EXPECT_TRUE(x.has_value());
        EXPECT_EQ(x.unwrap(), 4);
        this->reached_last.signal();
      });

  std::move(p).fulfill(2);
}

TYPED_TEST(ExceptionTests, promise_throw_in) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f)
      .template catch_error<std::runtime_error>([](auto&& e) {
        ADD_FAILURE() << "this should not be executed";
        return 5;
      })
      .then([](int x) {
        ADD_FAILURE() << "this should not be executed";
        return 2 * x;
      })
      .finally([&](expected<int> x) noexcept {
        EXPECT_TRUE(x.has_error());
        EXPECT_TRUE(x.has_exception<std::logic_error>());
        this->reached_last.signal();
      });

  std::move(p).throw_into(std::logic_error("This is a test exception"));
}

TYPED_TEST(ExceptionTests, promise_throw_exception) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f)
      .template catch_error<std::runtime_error>([](auto&& e) {
        ADD_FAILURE() << "this should not be executed";
        return 5;
      })
      .then([](int x) {
        ADD_FAILURE() << "this should not be executed";
        return 2 * x;
      })
      .finally([&](expected<int> x) noexcept {
        EXPECT_TRUE(x.has_error());
        EXPECT_TRUE(x.has_exception<std::logic_error>());
        this->reached_last.signal();
      });

  std::move(p).template throw_exception<std::logic_error>(
      "This is a test exception");
}

TYPED_TEST(ExceptionTests, promise_throw_in_catch) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f)
      .then([](int x) {
        ADD_FAILURE() << "this should not be executed";
        return 2 * x;
      })
      .template catch_error<std::logic_error>([](auto&& e) { return 5; })
      .finally([&](expected<int> x) noexcept {
        EXPECT_TRUE(x.has_value());
        EXPECT_EQ(x.unwrap(), 5);
        this->reached_last.signal();
      });

  std::move(p).throw_into(std::logic_error("This is a test exception"));
}

TYPED_TEST(ExceptionTests, promise_unwrap_or_exception) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f).unwrap_or(5).finally([&](int x) noexcept {
    EXPECT_EQ(x, 5);
    this->reached_last.signal();
  });

  std::move(p).throw_into(std::logic_error("This is a test exception"));
}

TYPED_TEST(ExceptionTests, promise_unwrap_or) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f).unwrap_or(5).finally([&](int x) noexcept {
    EXPECT_EQ(x, 12);
    this->reached_last.signal();
  });

  std::move(p).fulfill(12);
}

TYPED_TEST(ExceptionTests, future_then_exception) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f)
      .then([](int x) -> int { throw std::runtime_error("test error"); })
      .finally([&](auto&& e) noexcept {
        EXPECT_TRUE(e.has_error());
        EXPECT_TRUE(e.template has_exception<std::runtime_error>());
        this->reached_last.signal();
      });

  std::move(p).fulfill(12);
}

TYPED_TEST(ExceptionTests, future_then_expected_exception) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f)
      .then([](expected<int>&& x) -> int { throw std::runtime_error("test error"); })
      .finally([&](auto&& e) noexcept {
        EXPECT_TRUE(e.has_error());
        EXPECT_TRUE(e.template has_exception<std::runtime_error>());
        this->reached_last.signal();
      });

  std::move(p).fulfill(12);
}


TYPED_TEST(ExceptionTests, future_then_return_expected) {
  auto&& [f, p] = mellon::make_promise<expected<int>, TypeParam>();

  std::move(f)
      .then([](expected<int>&& x) -> expected<int> { return 5; }).flatten()
      .finally([&](expected<int>&& e) noexcept {
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.unwrap(), 5);
        this->reached_last.signal();
      });

  std::move(p).fulfill(12);
}
