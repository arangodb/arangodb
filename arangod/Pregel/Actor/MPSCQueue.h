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


namespace arangodb::pregel::mpscqueue {

struct MPSCQueue {
  struct Node {
    std::atomic<Node*> next{nullptr};
  };

  MPSCQueue() : stub{}, head(&stub), tail(&stub) {}

  auto push_internal(Node *value) -> void {
    auto prev = head.exchange(value);
    prev->next.store(value);
  }

  auto push(std::unique_ptr<Node> value) -> void {
    auto ptr = value.release();
    push_internal(ptr);
  }

  auto pop() -> std::unique_ptr<Node> {
    auto current = tail.load();
    auto next = current->next.load();

    if (current == &stub) {
      if (nullptr == next) {
        return nullptr;
      }
      tail.store(next);
      current = next;
      next = next->next.load();
    }

    if (next != nullptr) {
      tail.store(next);
      // FIXME
      return std::unique_ptr<Node>(current);
    }

    auto currenth = head.load();
    if (current != currenth) {
      return nullptr;
    }

    push_internal(&stub);

    next = current->next.load();

    if (next != nullptr) {
      tail.store(next);
      return std::unique_ptr<Node>(current);
    }

    return nullptr;
  }

  Node stub{};
  std::atomic<Node*> head;
  std::atomic<Node*> tail;
};

}  // namespace arangodb::pregel::mpscqueue
