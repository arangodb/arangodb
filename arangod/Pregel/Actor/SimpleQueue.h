////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <memory>
#include <atomic>
#include <deque>
#include <Basics/Guarded.h>

// this is the most trivial queue I could come up with.
namespace arangodb::pregel::simplequeue {

struct SimpleQueue {
  struct Node {
    virtual ~Node() = default;
  };
  SimpleQueue() = default;

  auto push(std::unique_ptr<Node> value) {
    auto ptr = value.release();
    queue.doUnderLock([ptr](auto& queue) {
      queue.emplace_back(ptr);
    });
  }

  auto pop() -> std::unique_ptr<Node> {
    return queue.doUnderLock([](auto& queue) -> std::unique_ptr<Node> {
      if (queue.empty()) {
        return nullptr;
      } else {
        auto front = queue.front();
        queue.pop_front();
        return std::unique_ptr<Node>(front);
      }
    });
  }

  arangodb::Guarded<std::deque<Node *>> queue;
};

}  // namespace arangodb::pregel::simplequeue
