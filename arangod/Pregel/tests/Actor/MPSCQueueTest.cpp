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
#include "Actor/MPSCQueue.h"

#include "fmt/core.h"



using namespace arangodb::pregel::mpscqueue;


struct Message : MPSCQueue::Node {
  Message(std::string content) : content(std::move(content)) {}
  std::string content;
};

TEST(MPSCQueue, gives_back_stuff_pushed) {
  auto queue = MPSCQueue();

  queue.push(std::make_unique<Message>("aon"));
  queue.push(std::make_unique<Message>("dha"));
  queue.push(std::make_unique<Message>("tri"));
  queue.push(std::make_unique<Message>("ceithir"));
  queue.push(std::make_unique<Message>("dannsa"));

  auto msg1 = static_cast<Message*>(queue.pop().release());
  ASSERT_EQ("aon", msg1->content);

  auto msg2 = static_cast<Message*>(queue.pop().release());
  ASSERT_EQ("dha", msg2->content);

  auto msg3 = static_cast<Message*>(queue.pop().release());
  ASSERT_EQ("tri", msg3->content);

  auto msg4 = static_cast<Message*>(queue.pop().release());
  ASSERT_EQ("ceithir", msg4->content);

  auto msg5 = static_cast<Message*>(queue.pop().release());
  ASSERT_EQ("dannsa", msg5->content);

  auto msg6 = static_cast<Message*>(queue.pop().release());
  ASSERT_EQ(nullptr, msg6);
}
