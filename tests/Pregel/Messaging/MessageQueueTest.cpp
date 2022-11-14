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
