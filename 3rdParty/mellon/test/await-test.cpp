#include "test-helper.h"
#include <thread>

struct AwaitTests : testing::Test {};

TEST(AwaitTests, await_fulfilled_future) {
  auto f = future<int>{std::in_place, 12};
  EXPECT_EQ(12, std::move(f).await());
}

TEST(AwaitTests, await_promise_future) {
  auto&&[f, p] = make_promise<int>();

  auto t = std::thread([p = std::move(p)]() mutable {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::move(p).fulfill(12);
  });

  EXPECT_EQ(12, std::move(f).await());
  t.join();
}

TEST(AwaitTests, await_future_fulfilled_timeout) {
  auto f = future<int>{std::in_place, 12};
  EXPECT_EQ(12, std::move(f).await_with_timeout(std::chrono::milliseconds(5)));
}


TEST(AwaitTests, await_promise_future_timeout) {
  auto&&[f, p] = make_promise<int>();
  auto result = std::move(f).await_with_timeout(std::chrono::milliseconds(1));
  EXPECT_FALSE(result.has_value());
  std::move(p).fulfill(12);
}
