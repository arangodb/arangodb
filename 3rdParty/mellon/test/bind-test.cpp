#include <mellon/futures.h>
#include "test-helper.h"

#include <string>

struct BindTests : ::testing::Test {
  signal_marker reached_last{"reached-last"};
};

TEST_F(BindTests, simple_bind_test) {
  auto&& [f1, p1] = make_promise<int>();
  auto&& [f2, p2] = make_promise<std::string>();

  std::move(f1)
      .bind([f2 = std::move(f2)](int x) mutable noexcept {
        EXPECT_EQ(x, 14);
        return std::move(f2);
      })
      .finally([this](std::string y) noexcept {
        EXPECT_EQ(y, "hello world");
        reached_last.signal();
      });

  std::move(p1).fulfill(14);
  std::move(p2).fulfill("hello world");
}

TEST_F(BindTests, bind_capture_test) {
  auto&& [f1, p1] = make_promise<int>();
  auto&& [f2, p2] = make_promise<std::string>();

  std::move(f1)
      .bind_capture([f2 = std::move(f2)](int x) mutable {
        EXPECT_EQ(x, 14);
        return std::move(f2);
      })
      .finally([this](expect::expected<std::string> y) noexcept {
        EXPECT_TRUE(y.has_value());
        EXPECT_EQ(y.unwrap(), "hello world");
        reached_last.signal();
      });

  std::move(p1).fulfill(14);
  std::move(p2).fulfill("hello world");
}

TEST_F(BindTests, bind_capture_except_test) {
  auto&& [f1, p1] = make_promise<int>();
  auto&& [f2, p2] = make_promise<std::string>();

  std::move(f1)
      .bind_capture([f2 = std::move(f2)](int x) mutable {
        EXPECT_EQ(x, 14);
        if (x == 13) {
          return std::move(f2);
        }
        throw std::runtime_error("test exception");
      })
      .finally([this](expect::expected<std::string>&& y) noexcept {
        EXPECT_TRUE(y.has_exception<std::runtime_error>());
        reached_last.signal();
      });

  std::move(p1).fulfill(14);
  std::move(p2).fulfill("hello world");
}

TEST_F(BindTests, then_bind_test) {
  auto&& [f1, p1] = make_promise<expect::expected<int>>();
  auto&& [f2, p2] = make_promise<std::string>();

  std::move(f1)
      .then_bind([f2 = std::move(f2)](int x) mutable {
        EXPECT_EQ(x, 13);
        if (x == 13) {
          return std::move(f2);
        }
        throw std::runtime_error("test exception");
      })
      .finally([this](expect::expected<std::string>&& y) noexcept {
        EXPECT_TRUE(y.has_value());
        EXPECT_EQ(y.unwrap(), "hello world");
        reached_last.signal();
      });

  std::move(p1).fulfill(13);
  std::move(p2).fulfill("hello world");
}

TEST_F(BindTests, then_bind_except_test_1) {
  auto&& [f1, p1] = make_promise<expect::expected<int>>();
  auto&& [f2, p2] = make_promise<std::string>();

  std::move(f1)
      .then_bind([f2 = std::move(f2)](int x) mutable {
        ADD_FAILURE() << "This should never be executed";
        if (x == 13) {
          return std::move(f2);
        }
        throw std::runtime_error("test exception");
      })
      .finally([this](expect::expected<std::string>&& y) noexcept {
        EXPECT_TRUE(y.has_exception<std::runtime_error>());
        reached_last.signal();
      });

  std::move(p1).throw_exception<std::runtime_error>("test exception");
  std::move(p2).fulfill("hello world");
}


TEST_F(BindTests, then_bind_expected_test) {
  auto&& [f1, p1] = make_promise<expect::expected<int>>();
  auto&& [f2, p2] = make_promise<expect::expected<std::string>>();

  std::move(f1)
      .then_bind([f2 = std::move(f2)](int x) mutable {
        EXPECT_EQ(x, 14);
        if (x == 13) {
          return std::move(f2);
        }
        throw std::runtime_error("test exception");
      }).then([](std::string&&) {
        ADD_FAILURE() << "This should never be executed";
        return 12;
      })
      .finally([this](expect::expected<int>&& y) noexcept {
        EXPECT_TRUE(y.has_exception<std::runtime_error>());
        reached_last.signal();
      });

  std::move(p1).fulfill(14);
  std::move(p2).throw_exception<std::runtime_error>("test exception");
}
