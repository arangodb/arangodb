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

namespace {
template<typename Runtime, typename A>
concept IncludesAllActorRelevantTypes =
    std::is_class<typename A::State>::value &&
    std::is_class<typename A::template Handler<Runtime>>::value &&
    std::is_class<typename A::Message>::value;
template<typename A>
concept IncludesName = requires() {
  { A::typeName() } -> std::convertible_to<std::string_view>;
};
template<typename Runtime, typename A>
concept HandlerInheritsFromBaseHandler =
    std::is_base_of < HandlerBase<Runtime, typename A::State>,
typename A::template Handler<Runtime> > ::value;
template<typename Runtime, typename A>
concept MessageIsVariant = requires(ActorPID pid,
                                    std::shared_ptr<Runtime> runtime,
                                    typename A::State state,
                                    typename A::Message message) {
  {std::visit(
      typename A::template Handler<Runtime>{
          {pid, pid, std::move(std::make_unique<typename A::State>(state)),
           runtime}},
      message)};
};
template<typename A>
concept IsInspectable = requires(typename A::Message message,
                                 typename A::State state) {
  {fmt::format("{}", message)};
  {fmt::format("{}", state)};
};
};  // namespace
template<typename Runtime, typename A>
concept Actorable = IncludesAllActorRelevantTypes<Runtime, A> &&
    IncludesName<A> && HandlerInheritsFromBaseHandler<Runtime, A> &&
    MessageIsVariant<Runtime, A> && IsInspectable<A>;

template<typename Runtime, typename Config>
requires Actorable<Runtime, Config>
struct Actor : ActorBase {
  Actor(ActorPID pid, std::shared_ptr<Runtime> runtime,
        std::unique_ptr<typename Config::State> initialState)
      : pid(pid), runtime(runtime), state(std::move(initialState)) {}

  Actor(ActorPID pid, std::shared_ptr<Runtime> runtime,
        std::unique_ptr<typename Config::State> initialState,
        std::size_t batchSize)
      : pid(pid),
        runtime(runtime),
        state(std::move(initialState)),
        batchSize(batchSize) {}

  ~Actor() = default;

  auto typeName() -> std::string_view override { return Config::typeName(); };

  void process(ActorPID sender,
               std::unique_ptr<MessagePayloadBase> msg) override {
    auto* ptr = msg.release();
    if (auto* m = dynamic_cast<MessagePayload<typename Config::Message>*>(ptr);
        m != nullptr) {
      push(sender, std::move(m->payload));
      delete m;
    } else if (auto* n = dynamic_cast<MessagePayload<ActorError>*>(ptr);
               n != nullptr) {
      push(sender, std::move(n->payload));
      delete n;
    } else {
      runtime->dispatch(
          pid, sender,
          ActorError{UnknownMessage{.sender = sender, .receiver = pid}});
      delete ptr;
    }
    kick();
  }

  void process(ActorPID sender, velocypack::SharedSlice msg) override {
    if (auto m =
            inspection::deserializeWithErrorT<typename Config::Message>(msg);
        m.ok()) {
      push(sender, std::move(m.get()));
    } else if (auto n = inspection::deserializeWithErrorT<ActorError>(msg);
               n.ok()) {
      push(sender, std::move(n.get()));
    } else {
      auto error =
          ActorError{UnknownMessage{.sender = sender, .receiver = pid}};
      auto payload = inspection::serializeWithErrorT(error);
      if (payload.ok()) {
        runtime->dispatch(pid, sender, payload.get());
      } else {
        fmt::print("Error serializing UnknownMessage");
        std::abort();
      }
    }
    kick();
  }

  auto serialize() -> velocypack::SharedSlice override {
    auto res = inspection::serializeWithErrorT(*this);
    if (not res.ok()) {
      std::abort();
    }
    return res.get();
  }

 private:
  void push(ActorPID sender, typename Config::Message&& msg) {
    inbox.push(std::make_unique<InternalMessage>(
        sender,
        std::make_unique<MessageOrError<typename Config::Message>>(msg)));
  }
  void push(ActorPID sender, ActorError&& msg) {
    inbox.push(std::make_unique<InternalMessage>(
        sender,
        std::make_unique<MessageOrError<typename Config::Message>>(msg)));
  }

  void kick() {
    // Make sure that *someone* works here
    (*runtime->scheduler)([this]() { this->work(); });
  }

  void work() {
    auto was_false = false;
    if (busy.compare_exchange_strong(was_false, true)) {
      auto i = batchSize;

      while (auto msg = inbox.pop()) {
        state = std::visit(
            typename Config::template Handler<Runtime>{
                {pid, msg->sender, std::move(state), runtime}},
            msg->payload->item);
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
    InternalMessage(
        ActorPID sender,
        std::unique_ptr<MessageOrError<typename Config::Message>>&& payload)
        : sender(sender), payload(std::move(payload)) {}
    ActorPID sender;
    std::unique_ptr<MessageOrError<typename Config::Message>> payload;
  };
  template<typename Inspector>
  auto inspect(Inspector& f, InternalMessage& x) {
    return f.object(x).fiels(f.field("sender", x.sender),
                             f.field("payload", x.payload));
  }

 public:
  ActorPID pid;
  std::atomic<bool> busy;
  arangodb::pregel::mpscqueue::MPSCQueue<InternalMessage> inbox;
  std::shared_ptr<Runtime> runtime;
  std::unique_ptr<typename Config::State> state;
  std::size_t batchSize{16};
};

// TODO make queue inspectable
template<typename Runtime, typename Config, typename Inspector>
requires Actorable<Runtime, Config>
auto inspect(Inspector& f, Actor<Runtime, Config>& x) {
  return f.object(x).fields(f.field("pid", x.pid), f.field("state", x.state),
                            f.field("batchsize", x.batchSize));
}

}  // namespace arangodb::pregel::actor

template<typename Runtime, typename Config>
requires arangodb::pregel::actor::Actorable<Runtime, Config>
struct fmt::formatter<arangodb::pregel::actor::Actor<Runtime, Config>>
    : arangodb::inspection::inspection_formatter {
};
