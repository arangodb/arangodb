#include "gtest/gtest.h"

#include "Pregel/Messaging/MessageQueue.h"

using namespace arangodb;
using namespace arangodb::pregel;

TEST(PregelMessageQueue, pops_first_item) {
  auto queue = MessageQueue<int>{};
  queue.push(2);
  queue.push(3);
  ASSERT_EQ(queue.pop(), 2);
  ASSERT_EQ(queue.pop(), 3);
}

TEST(PregelMessageQueue, pushes_item) {
  auto queue = MessageQueue<int>{};
  queue.push(6);
  ASSERT_EQ(queue.pop(), 6);
}

TEST(PregelMessageQueue, pushes_item_at_end) {
  auto queue = MessageQueue<int>{};
  queue.push(3);
  queue.push(6);
  ASSERT_EQ(queue.pop(), 3);
  ASSERT_EQ(queue.pop(), 6);
}

TEST(PregelMessageQueue, waits_for_item_if_queue_is_empty) {
  using namespace std::chrono_literals;
  auto queue = MessageQueue<int>{};
  queue.push(9);
  std::thread other_thread([&]() {
    std::this_thread::sleep_for(1000ms);
    queue.push(3);
  });
  ASSERT_EQ(queue.pop(), 9);
  other_thread.join();
  ASSERT_EQ(queue.pop(), 3);
}

TEST(PregelMessageQueue, can_push_from_front_and_pop_to_back_simulatneously) {
  auto queue = MessageQueue<int>{};
  queue.push(5);
  std::thread other_thread([&]() { queue.push(8); });
  ASSERT_EQ(queue.pop(), 5);
  other_thread.join();
  ASSERT_EQ(queue.pop(), 8);
}
