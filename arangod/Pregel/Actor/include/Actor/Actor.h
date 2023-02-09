///////////////////////////////////////////////////////////////////////////////
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
#include <optional>
#include <string_view>
#include <type_traits>

#include "Actor/ActorBase.h"
#include "Actor/Assert.h"
#include "Actor/HandlerBase.h"
#include "Actor/Message.h"
#include "Actor/MPSCQueue.h"

namespace arangodb::pregel::actor {

namespace {
template<typename Runtime, typename A>
concept IncludesAllActorRelevantTypes = std::is_class_v<typename A::State> &&
    std::is_class_v<typename A::template Handler<Runtime>> &&
    std::is_class_v<typename A::Message>;
template<typename A>
concept IncludesName = requires() {
  { A::typeName() } -> std::convertible_to<std::string_view>;
};
template<typename Runtime, typename A>
concept HandlerInheritsFromBaseHandler =
    std::is_base_of_v < HandlerBase<Runtime, typename A::State>,
typename A::template Handler<Runtime> > ;
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
}  // namespace
template<typename Runtime, typename A>
concept Actorable = IncludesAllActorRelevantTypes<Runtime, A> &&
    IncludesName<A> && HandlerInheritsFromBaseHandler<Runtime, A> &&
    MessageIsVariant<Runtime, A> && IsInspectable<A>;

template<typename Runtime, typename Config>
requires Actorable<Runtime, Config>
struct Actor : ActorBase, std::enable_shared_from_this<Actor<Runtime, Config>> {
  Actor(ActorPID pid, std::shared_ptr<Runtime> runtime,
        std::unique_ptr<typename Config::State> initialState)
      : pid(pid), runtime(runtime), state(std::move(initialState)) {}

  Actor(ActorPID pid, std::shared_ptr<Runtime> runtime,
        std::unique_ptr<typename Config::State> initialState,
        std::size_t batchSize)
      : pid(pid),
        runtime(runtime),
        batchSize(batchSize),
        state(std::move(initialState)) {}

  ~Actor() = default;

  auto typeName() -> std::string_view override { return Config::typeName(); };

  void process(ActorPID sender, MessagePayloadBase& msg) override {
    if (auto* m = dynamic_cast<MessagePayload<typename Config::Message>*>(&msg);
        m != nullptr) {
      push(sender, std::move(m->payload));
    } else if (auto* n = dynamic_cast<MessagePayload<ActorError>*>(&msg);
               n != nullptr) {
      push(sender, std::move(n->payload));
    } else {
      runtime->dispatch(
          pid, sender,
          ActorError{UnknownMessage{.sender = sender, .receiver = pid}});
    }
  }

  void process(ActorPID sender, velocypack::SharedSlice const msg) override {
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
      ACTOR_ASSERT(payload.ok());
      runtime->dispatch(pid, sender, payload.get());
    }
  }

  auto finish() -> void override { finished.store(true); }
  auto isFinishedAndIdle() -> bool override {
    return finished.load() and idle.load();
  }

  auto isIdle() -> bool override { return idle.load(); }

  auto serialize() -> velocypack::SharedSlice override {
    auto res = inspection::serializeWithErrorT(*this);
    ACTOR_ASSERT(res.ok());
    return res.get();
  }

  auto getState() -> std::optional<typename Config::State> {
    auto s = state.get();
    if (s == nullptr) {
      return std::nullopt;
    }
    return *s;
  }

  template<typename R, typename C, typename Inspector>
  requires Actorable<R, C>
  friend auto inspect(Inspector& f, Actor<R, C>& x);

 private:
  void push(ActorPID sender, typename Config::Message&& msg) {
    pushToQueueAndKick(std::make_unique<InternalMessage>(
        sender,
        std::make_unique<MessageOrError<typename Config::Message>>(msg)));
  }
  void push(ActorPID sender, ActorError&& msg) {
    pushToQueueAndKick(std::make_unique<InternalMessage>(
        sender,
        std::make_unique<MessageOrError<typename Config::Message>>(msg)));
  }

  void kick() {
    // Make sure that *someone* works here

    // capture 'this' as weak ptr: this way, actor can be destroyed although
    // this callback is still waiting in the scheduler. When this callback is
    // executed in the queue after actor was destroyed, the weak ptr will
    // be empty and work will just not be executed
    (*runtime->scheduler)([self = this->weak_from_this()]() {
      auto me = self.lock();
      if (me != nullptr) {
        me->work();
      }
    });
  }

  void work() {
    auto i = batchSize;

    while (auto msg = inbox.pop()) {
      state = std::visit(
          typename Config::template Handler<Runtime>{
              {pid, msg->sender, std::move(state), runtime}},
          msg->payload->item);
      if (--i == 0) {
        break;
      }
    }

    // push more work to scheduler if queue is still not empty
    if (not inbox.empty()) {
      kick();
      return;
    }

    idle.store(true);

    // push more work to scheduler if a message was added to queue after
    // previous inbox.empty check and set idle to false
    auto isIdle = true;
    if (not inbox.empty() and idle.compare_exchange_strong(isIdle, false)) {
      kick();
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

  auto pushToQueueAndKick(std::unique_ptr<InternalMessage> msg) -> void {
    // don't add new messages when actor is finished
    if (finished.load()) {
      return;
    }

    inbox.push(std::move(msg));

    // only push work to scheduler if actor is idle (meaning no work is waiting
    // on the scheduler and no work is currently processed in work())
    // and set idle to false
    auto isIdle = true;
    if (idle.compare_exchange_strong(isIdle, false)) {
      kick();
    }
  }

  ActorPID pid;
  std::atomic<bool> idle{true};
  std::atomic<bool> finished{false};
  arangodb::pregel::mpscqueue::MPSCQueue<InternalMessage> inbox;
  std::shared_ptr<Runtime> runtime;
  // tunable parameter: maximal number of processed messages per work() call
  std::size_t batchSize{16};
  std::unique_ptr<typename Config::State> state;
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
