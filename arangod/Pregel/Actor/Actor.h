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

namespace arangodb::pregel::actor {

template<typename Scheduler, typename MessageHandler, typename State,
         typename Message>
struct Actor {
  Actor(Scheduler& schedule, State initialState)
      : schedule(schedule), state(initialState) {}

  Actor(Scheduler& schedule, State initialState, std::size_t batchSize)
      : batchSize(batchSize), schedule(schedule), state(initialState) {}


  void process(Message msg) {
    inbox.push(new Message(msg));
    kick();
  }

  void kick() {
    // Make sure that *someone* works here
    schedule([this]() { this->work(); });
  }

  void work() {
    auto was_false = false;
    if (busy.compare_exchange_strong(was_false, true)) {
      // TODO: this needs to be a constructor parameter
      auto i = size_t{5};
      Message* msg = nullptr;
      while (inbox.pop(msg) and i > 0) {
        // TODO: assert msg is not a nullptr.
        state = handler(state, *msg);
        // there'd better not be an exception happening anywhere her
        delete msg;
        i--;
      }
      if (i == 0 && !inbox.empty()) {
        kick();
      }
      busy.store(false);
    }
  }

  std::size_t batchSize{16};
  std::atomic<bool> busy;
  // TODO: Waserlaube boost::lockfree::queue? One can only use
  // trivially destructible members, so we need to use raw pointers
  // here, like cavepeople.
  boost::lockfree::queue<Message*> inbox;
  Scheduler& schedule;
  MessageHandler handler{};
  State state;
};

template<typename Scheduler, typename MessageHandler, typename State,
         typename Message>
void send(Actor<Scheduler, MessageHandler, State, Message>& actor,
          Message msg) {
  actor.process(msg);
}

}  // namespace arangodb::pregel::actor
