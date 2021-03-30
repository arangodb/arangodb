#include <cassert>
#include <iostream>
#include <thread>

#include <mellon/completion-queue.h>
#include <mellon/futures.h>
#include <mellon/utilities.h>

#include "test-helper.h"

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
