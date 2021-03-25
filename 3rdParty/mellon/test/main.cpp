#include <cassert>
#include <iostream>
#include <thread>

#include <mellon/completion-queue.h>
#include <mellon/futures.h>
#include <mellon/utilities.h>

#include "test-helper.h"

using namespace expect;


struct FutureTests {};

TEST(FutureTests, simple_test) {}

TEST(FutureTests, simple_abandon_before) {
  auto&& [future, promise] = mellon::make_promise<int, mellon::default_tag>();
  std::move(promise).abandon();
  EXPECT_DEATH(std::move(future).finally(
                   [&](int x) noexcept { EXPECT_EQ(x, 12); }),
               "");
  std::move(future).abandon();
}

TEST(FutureTests, simple_abandon_after) {
  auto&& [future, promise] = mellon::make_promise<int, mellon::default_tag>();
  std::move(future).finally([&](int x) noexcept { EXPECT_EQ(x, 12); });
  EXPECT_DEATH(std::move(promise).abandon(), "");
  std::move(promise).fulfill(12);
}

TEST(FutureTests, expected_throw_test) {
  {
    auto&& [future, promise] = make_promise<expected<int>>();

    std::move(promise).throw_exception<std::runtime_error>("test");
    std::move(future)
        .then([&](int x) {
          ADD_FAILURE() << "This should never be executed";
          return 2 * x;
        })
        .catch_error<std::runtime_error>([&](auto&& e) { return 5; })
        .finally([&](expected<int>&& e) noexcept {
          EXPECT_TRUE(e.has_value());
          EXPECT_EQ(e.unwrap(), 5);
        });
  }

  {
    auto&& [future, promise] = make_promise<expected<int>>();

    std::move(future)
        .then([&](int x) {
          ADD_FAILURE() << "This should never be executed";
          return 2 * x;
        })
        .catch_error<std::runtime_error>([&](auto&& e) { return 5; })
        .finally([&](expected<int>&& e) noexcept {
          EXPECT_TRUE(e.has_value());
          EXPECT_EQ(e.unwrap(), 5);
        });

    std::move(promise).throw_exception<std::runtime_error>("test");
  }
}

TEST(FutureTests, expected_no_throw_test) {
  auto&& [future, promise] = make_promise<expected<int>>();

  std::move(promise).fulfill(3);
  std::move(future)
      .then([&](int x) { return 2 * x; })
      .catch_error<std::runtime_error>([&](auto&& e) {
        ADD_FAILURE() << "This should never be executed";
        return 5;
      })
      .finally([&](expected<int>&& e) noexcept {
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.unwrap(), 6);
      });
}

TEST(FutureTests, fulfill_midway) {
  auto&& [future, promise] = make_promise<int>();

  bool executed_last = false;

  auto f2 = std::move(future).and_then_direct([](int x) noexcept {
    EXPECT_EQ(x, 3);
    return x * 2;
  });

  std::move(promise).fulfill(3);

  std::move(f2).finally([&](int x) noexcept {
    EXPECT_EQ(x, 6);
    executed_last = true;
  });

  EXPECT_TRUE(executed_last);
}

TEST(FutureTests, fulfill_in_vector) {
  auto&& [f1, p] = make_promise<int>();

  bool executed_last = false;

  std::vector<future<int>> v;

  v.emplace_back(std::move(f1).and_then([](int x) noexcept {
     return 2 * x;
   }));

  std::move(p).fulfill(3);

  std::move(v.back()).finally([&](int x) noexcept {
    EXPECT_EQ(x, 6);
    executed_last = true;
  });

  EXPECT_TRUE(executed_last);
}

void print_exception(const std::exception& e, int level = 0) {
  std::cerr << std::string(level, ' ') << "exception of type "
            << typeid(e).name() << ": " << e.what() << '\n';
  try {
    std::rethrow_if_nested(e);
  } catch (const std::exception& e) {
    print_exception(e, level + 1);
  } catch (...) {
  }
}

TEST(FutureTests, expected_rethrow_nested) {
  auto&& [future, promise] = make_promise<expected<int>>();

  std::move(promise).throw_exception<std::runtime_error>("fail");
  std::move(future)
      .rethrow_nested_if<std::runtime_error, std::logic_error>(
          "exceptions are not allowed")
      .catch_error<std::runtime_error>([](auto&& e) {
        ADD_FAILURE() << "This should never be executed";
        return 1;
      })
      .finally([](expected<int>&& e) noexcept {
        EXPECT_TRUE(e.has_exception<std::logic_error>());
      });
}

template <typename T>
struct convertible_from_T {
  convertible_from_T(T t) : t(std::move(t)) {}
  T& value() { return t; }

 private:
  T t;
};

template <typename T>
struct convertible_into_T {
  virtual operator T() { return t; };
  explicit convertible_into_T(T t) : t(std::move(t)) {}
  T& value() { return t; }

 private:
  T t;
};

TEST(FutureTests, as_something) {
  auto&& [future, promise] = make_promise<int>();

  std::move(future).as<convertible_from_T<int>>().finally(
      [](convertible_from_T<int>&& e) noexcept { EXPECT_EQ(e.value(), 10); });

  std::move(promise).fulfill(10);
}

TEST(FutureTests, as_something_from) {
  auto&& [future, promise] = make_promise<convertible_into_T<int>>();

  std::move(future).as<int>().finally([](int e) noexcept { EXPECT_EQ(e, 10); });

  std::move(promise).fulfill(10);
}

struct CollectTest {};

TEST(CollectTest, collect_vector) {
  bool reached = false;
  auto&& [f1, p1] = make_promise<int>();
  auto&& [f2, p2] = make_promise<int>();

  auto fs = std::vector<future<int>>{};
  auto ps = std::vector<promise<int>>{};

  const auto number_of_futures = 4;

  for (size_t i = 0; i < number_of_futures; i++) {
    auto&& [f, p] = make_promise<int>();
    fs.emplace_back(std::move(f).and_then([i](int x) noexcept { return i * x; }));
    ps.emplace_back(std::move(p));
  }

  collect(fs.begin(), fs.end()).finally([&](auto&& xs) noexcept {
    EXPECT_EQ(xs.size(), number_of_futures);
    for (size_t i = 0; i < number_of_futures; i++) {
      EXPECT_EQ(xs[i], i);
    }
    reached = true;
  });

  for (auto&& p : ps) {
    std::move(p).fulfill(1);
  }
  EXPECT_TRUE(reached);
}

TEST(CollectTest, collect_tuple) {
  bool reached = false;
  auto&& [f1, p1] = make_promise<int>();
  auto&& [f2, p2] = make_promise<int>();

  collect(std::move(f1), std::move(f2)).finally([&](auto&& x) noexcept {
    EXPECT_EQ(std::get<0>(x), 1);
    EXPECT_EQ(std::get<1>(x), 2);
    reached = true;
  });

  std::move(p2).fulfill(2);
  std::move(p1).fulfill(1);
  EXPECT_TRUE(reached);
}

struct HandlerTest {
  struct tag {};

  template<typename T>
  struct handler {

    T operator()() noexcept {
      was_called = true;
      return {};
    }
    void operator()(T && t) noexcept {
      was_called = true;
    }

    static bool was_called;
  };

};
using handler_test_tag = HandlerTest::tag;

template <>
struct mellon::tag_trait<handler_test_tag> : mellon::tag_trait<default_test_tag> {
  template<typename T>
  using abandoned_future_handler = HandlerTest::handler<T>;
  template<typename T>
  using abandoned_promise_handler = HandlerTest::handler<T>;
};


template <typename T>
bool HandlerTest::handler<T>::was_called = false;

TEST(HandlerTest, test_abandoned_promise_handler) {
  auto&& [f, p] = mellon::make_promise<int, handler_test_tag>();

  std::move(f).finally([](int x) noexcept {
    EXPECT_EQ(x, 0);  // should be default constructed by the handler
  });

  HandlerTest::handler<int>::was_called = false;
  std::move(p).abandon();
  ASSERT_TRUE(HandlerTest::handler<int>::was_called);
}

TEST(HandlerTest, test_abandoned_promise_handler2) {
  auto&& [f, p] = mellon::make_promise<int, handler_test_tag>();

  HandlerTest::handler<int>::was_called = false;
  std::move(p).abandon();
  std::move(f).finally([](int x) noexcept {
    EXPECT_EQ(x, 0);  // should be default constructed by the handler
  });

  ASSERT_TRUE(HandlerTest::handler<int>::was_called);
}

TEST(HandlerTest, test_abandoned_promise_handler3) {
  auto&& [f, p] = mellon::make_promise<int, handler_test_tag>();

  HandlerTest::handler<int>::was_called = false;
  std::move(p).abandon();
  std::move(f).and_then_direct([](int x) noexcept {
    EXPECT_EQ(x, 0);  // should be default constructed by the handler
    return 15;
  }).finally([](auto) noexcept {});

  ASSERT_TRUE(HandlerTest::handler<int>::was_called);
}

TEST(HandlerTest, test_abandoned_future_handler) {
  auto&& [f, p] = mellon::make_promise<int, handler_test_tag>();
  std::move(p).fulfill(1);
  HandlerTest::handler<int>::was_called = false;
  std::move(f).abandon();
  ASSERT_TRUE(HandlerTest::handler<int>::was_called);
}

TEST(HandlerTest, test_abandoned_future_handler_2) {
  auto&& [f, p] = mellon::make_promise<int, handler_test_tag>();
  std::move(f).abandon();
  HandlerTest::handler<int>::was_called = false;
  std::move(p).fulfill(1);
  ASSERT_TRUE(HandlerTest::handler<int>::was_called);
}



int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
