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
#include <string_view>
#include <type_traits>

#include "ActorBase.h"
#include "HandlerBase.h"
#include "Message.h"
#include "MPSCQueue.h"

namespace arangodb::pregel::actor {

struct Dispatcher;

namespace {
template<typename A>
concept IncludesAllActorRelevantTypes =
    std::is_class<typename A::State>::value &&
    std::is_class<typename A::Handler>::value &&
    std::is_class<typename A::Message>::value;
template<typename A>
concept IncludesName = requires() {
  { A::typeName() } -> std::convertible_to<std::string_view>;
};
template<typename A>
concept HandlerInheritsFromBaseHandler =
    std::is_base_of < HandlerBase<typename A::State>,
typename A::Handler > ::value;
template<typename A>
concept MessageIsVariant =
    requires(ActorPID pid, std::shared_ptr<Dispatcher> messageDispatcher,
             typename A::State state, typename A::Message message) {
  {std::visit(
      typename A::Handler{
          {pid, pid, std::move(std::make_unique<typename A::State>(state)),
           messageDispatcher}},
      message)};
};
template<typename A>
concept IsInspectable = requires(typename A::Message message,
                                 typename A::State state) {
  {fmt::format("{}", message)};
  {fmt::format("{}", state)};
};
};  // namespace
template<typename A>
concept Actorable = IncludesAllActorRelevantTypes<A> && IncludesName<A> &&
    HandlerInheritsFromBaseHandler<A> && MessageIsVariant<A> &&
    IsInspectable<A>;

template<typename S>
concept CallableOnFunction = requires(S s) {
  {s([]() {})};
};

template<CallableOnFunction Scheduler, Actorable Config>
struct Actor : ActorBase {
  Actor(ActorPID pid, std::shared_ptr<Scheduler> schedule,
        std::shared_ptr<Dispatcher> dispatcher,
        std::unique_ptr<typename Config::State> initialState)
      : pid(pid),
        schedule(schedule),
        messageDispatcher(dispatcher),
        state(std::move(initialState)) {}

  Actor(ActorPID pid, std::shared_ptr<Scheduler> schedule,
        std::shared_ptr<Dispatcher> dispatcher,
        std::unique_ptr<typename Config::State> initialState,
        std::size_t batchSize)
      : pid(pid),
        schedule(schedule),
        messageDispatcher(dispatcher),
        state(std::move(initialState)),
        batchSize(batchSize) {}

  ~Actor() = default;

  auto typeName() -> std::string_view override { return Config::typeName(); };

  void process(ActorPID sender,
               std::unique_ptr<MessagePayloadBase> msg) override {
    auto* ptr = msg.release();
    auto* m = dynamic_cast<MessagePayload<typename Config::Message>*>(ptr);
    if (m == nullptr) {
      // TODO possibly send an information back to the runtime
      fmt::print("Actor {} recieved a message it could not handle from {}", pid,
                 sender);
      std::abort();
    }
    inbox.push(std::make_unique<InternalMessage>(
        sender,
        std::make_unique<typename Config::Message>(std::move(m->payload))));
    delete m;
    kick();
  }
  void process(ActorPID sender, velocypack::SharedSlice msg) override {
    auto m = inspection::deserializeWithErrorT<typename Config::Message>(msg);

    if (m.ok()) {
      process(
          sender,
          std::make_unique<MessagePayload<typename Config::Message>>(m.get()));
    } else {
      std::abort();
    }
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
        state = std::visit(
            typename Config::Handler{
                {pid, msg->sender, std::move(state), messageDispatcher}},
            *msg->payload);
        i--;
        if (i == 0) {
          break;
        }
      }
      busy.store(false);

      if (!inbox.empty()) {
        kick();
      }
    }
  }

  struct InternalMessage
      : arangodb::pregel::mpscqueue::MPSCQueue<InternalMessage>::Node {
    InternalMessage(ActorPID sender,
                    std::unique_ptr<typename Config::Message>&& payload)
        : sender(sender), payload(std::move(payload)) {}
    ActorPID sender;
    std::unique_ptr<typename Config::Message> payload;
  };
  template<typename Inspector>
  auto inspect(Inspector& f, InternalMessage& x) {
    return f.object(x).fiels(f.field("sender", x.sender),
                             f.field("payload", x.payload));
  }

  ActorPID pid;
  std::atomic<bool> busy;
  arangodb::pregel::mpscqueue::MPSCQueue<InternalMessage> inbox;
  std::shared_ptr<Scheduler> schedule;
  std::shared_ptr<Dispatcher> messageDispatcher;
  std::unique_ptr<typename Config::State> state;
  std::size_t batchSize{16};
};

// TODO make queue inspectable
template<CallableOnFunction Scheduler, Actorable Config, typename Inspector>
auto inspect(Inspector& f, Actor<Scheduler, Config>& x) {
  return f.object(x).fields(f.field("pid", x.pid), f.field("state", x.state),
                            f.field("batchsize", x.batchSize));
}

}  // namespace arangodb::pregel::actor

template<arangodb::pregel::actor::CallableOnFunction Scheduler,
         arangodb::pregel::actor::Actorable Config>
struct fmt::formatter<arangodb::pregel::actor::Actor<Scheduler, Config>>
    : arangodb::inspection::inspection_formatter {};
