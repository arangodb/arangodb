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
#include <memory>
#include <iostream>

#include "ActorPID.h"
#include "Message.h"
#include "MPSCQueue.h"


namespace arangodb::pregel::actor {

struct ActorBase {
  virtual ~ActorBase() = default;
  virtual auto process(ActorPID sender, std::unique_ptr<MessagePayloadBase> payload) -> void = 0;
  // virtual auto typeName() -> std::string = 0;
  // state: initialised, running, finished
};

template<typename Scheduler, typename MessageHandler, typename State,
         typename ActorMessage>
struct Actor : ActorBase {
  Actor(std::shared_ptr<Scheduler> schedule, std::unique_ptr<State> initialState)
      : schedule(schedule), state(std::move(initialState)) {}

  Actor(std::shared_ptr<Scheduler> schedule, std::unique_ptr<State> initialState,
        std::size_t batchSize)
      : batchSize(batchSize), schedule(schedule) {}

  ~Actor() = default;

  void process(ActorPID sender, std::unique_ptr<MessagePayloadBase> msg) override {
    auto* ptr = msg.release();
    auto* m = dynamic_cast<MessagePayload<ActorMessage>*>(ptr);
    if(m == nullptr) {
      std::cout << "actor process found a nullptr payload" << std::endl;
      std::abort();
      // ADB_PROD_ASSERT(false);
      // platzen
    }
    // TODO: we're leaking m
    inbox.push(std::make_unique<InternalMessage>(sender, std::make_unique<ActorMessage>(m->payload)));
    kick();
  }

  void kick() {
    // Make sure that *someone* works here
    (*schedule)([this]() { this->work(); });
  }

  void work() {
    auto was_false = false;
    if (busy.compare_exchange_strong(was_false, true)) {
      auto i = batchSize;

      while (auto msg = inbox.pop()) {
        if(msg == nullptr) {
          std::cout << "work msg was nullptr" << std::endl;
        }
        if(msg->payload == nullptr) {
          std::cout << "work payload was nullptr" << std::endl;
        }

        state = std::visit(MessageHandler{std::move(state)}, *msg->payload);
        i--;
        if (i == 0) {
          break;
        }
      }
      busy.store(false);

      if(!inbox.empty()) {
        kick();
      }
    }
  }

  struct InternalMessage : arangodb::pregel::mpscqueue::MPSCQueue<InternalMessage>::Node {
    InternalMessage(ActorPID sender, std::unique_ptr<ActorMessage>&& payload) : sender(sender), payload(std::move(payload)) {}
    ActorPID sender;
    std::unique_ptr<ActorMessage> payload;
  };

  std::size_t batchSize{16};
  std::atomic<bool> busy;
  arangodb::pregel::mpscqueue::MPSCQueue<InternalMessage> inbox;
  std::shared_ptr<Scheduler> schedule;
  std::unique_ptr<State> state;
};

// template<typename Scheduler, typename MessageHandler, typename State,
//          typename Message>
// void send(Actor<Scheduler, MessageHandler, State, Message>& actor,
//           std::unique_ptr<Message> msg) {
//   actor.process(std::move(msg));
// }

}  // namespace arangodb::pregel::actor
