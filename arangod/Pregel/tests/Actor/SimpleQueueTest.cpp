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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <thread>
#include <string>

#include "gtest/gtest.h"
#include "Pregel/Actor/SimpleQueue.h"
#include "Basics/ThreadGuard.h"

#include "fmt/core.h"

using namespace arangodb;
using namespace arangodb::pregel::simplequeue;

struct SimpleStringMessage : SimpleQueue<SimpleStringMessage>::Node {
  SimpleStringMessage(std::string content) : content(std::move(content)) {}
  std::string content;
};

// this test just pushes and pops some messages to see whether the
// queue works properly in the absence of
// concurrency.
TEST(SimpleQueue, gives_back_stuff_pushed) {
  auto queue = SimpleQueue<SimpleStringMessage>();

  queue.push(std::make_unique<SimpleStringMessage>("aon"));
  queue.push(std::make_unique<SimpleStringMessage>("dha"));
  queue.push(std::make_unique<SimpleStringMessage>("tri"));

  ASSERT_EQ("aon", queue.pop()->content);
  ASSERT_EQ("dha", queue.pop()->content);
  ASSERT_EQ("tri", queue.pop()->content);

  // here the queue should be empty
  ASSERT_EQ(nullptr, queue.pop());

  queue.push(std::make_unique<SimpleStringMessage>("ceithir"));
  queue.push(std::make_unique<SimpleStringMessage>("dannsa"));

  ASSERT_EQ("ceithir", queue.pop()->content);
  ASSERT_EQ("dannsa", queue.pop()->content);

  // empty again!
  ASSERT_EQ(nullptr, queue.pop());

  queue.push(std::make_unique<SimpleStringMessage>("coig"));

  ASSERT_EQ("coig", queue.pop()->content);

  // not empty
  queue.push(std::make_unique<SimpleStringMessage>("sia"));
  ASSERT_EQ("sia", queue.pop()->content);

  ASSERT_EQ(nullptr, queue.pop());
}

struct SimpleThreadMessage : SimpleQueue<SimpleThreadMessage>::Node {
  SimpleThreadMessage(size_t threadId, size_t messageId)
      : threadId(threadId), messageId(messageId) {}
  size_t threadId{};
  size_t messageId{};
};

// This test starts a number of system threads that push
// messages onto the message queue, and an additional
// thread that keeps reading messages from the queue;
//
// Apart from checking that this process doesn't crash
// the test checks that every message id from every thread
// has been read in the consumer.
TEST(SimpleQueue, threads_push_stuff_comes_out) {
  constexpr auto numberThreads = size_t{125};
  constexpr auto numberMessages = size_t{10000};

  auto queue = SimpleQueue<SimpleThreadMessage>();
  auto threads = ThreadGuard();

  for (auto threadId = size_t{0}; threadId < numberThreads; ++threadId) {
    threads.emplace([&queue, threadId]() {
      for (size_t msgN = 0; msgN < numberMessages; ++msgN) {
        queue.push(std::make_unique<SimpleThreadMessage>(threadId, msgN));
      }
    });
  }

  auto receivedIds = std::vector<std::vector<bool>>(numberThreads);
  for (auto& thread : receivedIds) {
    thread = std::vector<bool>(numberMessages);
  }

  threads.emplace([&queue, &receivedIds, numberThreads]() {
    auto counter = size_t{0};

    while (true) {
      auto msg = queue.pop();
      if (msg != nullptr) {
        receivedIds[msg->threadId][msg->messageId] = true;
        counter++;

        ASSERT_LT(msg->threadId, numberThreads);
        ASSERT_LE(counter, numberThreads * numberMessages);

        if (counter == numberThreads * numberMessages) {
          break;
        }
      }
    }
  });
  threads.joinAll();

  for (auto& thread : receivedIds) {
    ASSERT_TRUE(std::all_of(std::begin(thread), std::end(thread),
                            [](auto x) { return x; }));
  }
}
