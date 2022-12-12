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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <Inspection/VPackWithErrorT.h>
#include <memory>

#include "ActorPID.h"
#include "MPSCQueue.h"

namespace arangodb::pregel::actor {

struct MessagePayloadBase {
  virtual ~MessagePayloadBase() = default;
};

struct Message {
  Message(ActorPID sender, ActorPID receiver,
          std::unique_ptr<MessagePayloadBase> payload)
      : sender(sender), receiver(receiver), payload(std::move(payload)) {}

  ActorPID sender;
  ActorPID receiver;

  std::unique_ptr<MessagePayloadBase> payload;
};

}  // namespace arangodb::pregel::actor
