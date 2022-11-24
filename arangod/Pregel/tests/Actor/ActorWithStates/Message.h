#pragma once
#include <variant>
#include "Actor/Actor.h"

namespace arangodb::pregel::actor {

struct InitStart {};
struct InitDone {};
struct InitConductor {};
struct MessagePayload : std::variant<InitStart, InitDone, InitConductor> {
  using std::variant<InitStart, InitDone, InitConductor>::variant;
};
struct Message : public MessagePayload,
                 public mpscqueue::MPSCQueue<Message>::Node,
                 ActorMessageBase {
  using MessagePayload::MessagePayload;
  Message(std::shared_ptr<ActorBase> base, MessagePayload payload)
      : MessagePayload(std::move(payload)), ActorMessageBase(base) {}
};

}  // namespace arangodb::pregel::actor
