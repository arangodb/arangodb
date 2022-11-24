#pragma once
#include <variant>
#include "Actor/Actor.h"

namespace arangodb::pregel::actor {

struct InitStart {};
struct InitDone {};
struct MessagePayload : std::variant<InitStart, InitDone> {
  using std::variant<InitStart, InitDone>::variant;
};
struct Message : public MessagePayload,
                 public mpscqueue::MPSCQueue<Message>::Node,
                 ActorMessageBase {
  using MessagePayload::MessagePayload;
};

}  // namespace arangodb::pregel::actor
