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

#include <Inspection/VPackWithErrorT.h>
#include <boost/lockfree/queue.hpp>

namespace arangodb::pregel {

template<typename Scheduler, typename MessagableT, typename MessageT>
struct Messagable {
  template<typename... Args>
  Messagable(Scheduler* scheduler, Args&&... args)
      : scheduler(scheduler), contained(std::forward<Args>(args)...) {}

  void enqueue(MessageT msg) {
    inbox.push(msg);
    kick();
  }

  void kick() {
    // Make sure that *someone* works here
    scheduler->queue([this]() { this->work(); });
  }

  void work() {
    auto was_false = false;
    if (busy.compare_exchange_strong(was_false, true)) {
      // TODO: this needs to be a constructor parameter
      auto i = size_t{5};
      auto msg = MessageT{};
      while (inbox.pop(msg) and i > 0) {
        contained.handle(msg);
        i--;
      }
      if (i == 0 && !inbox.empty()) {
        kick();
      }
      busy.store(false);
    }
  }

  std::atomic<bool> busy;
  boost::lockfree::queue<MessageT> inbox;
  Scheduler* scheduler;
  MessagableT contained;
};
}  // namespace arangodb::pregel
