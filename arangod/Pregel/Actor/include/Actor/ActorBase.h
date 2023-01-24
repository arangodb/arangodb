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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Actor/ActorPID.h"
#include "Actor/Message.h"

namespace arangodb::pregel::actor {

struct ActorBase {
  virtual ~ActorBase() = default;
  virtual auto process(ActorPID sender,
                       std::unique_ptr<MessagePayloadBase> payload) -> void = 0;
  virtual auto process(ActorPID sender, velocypack::SharedSlice msg)
      -> void = 0;
  virtual auto typeName() -> std::string_view = 0;
  virtual auto serialize() -> velocypack::SharedSlice = 0;
  virtual auto finish() -> void = 0;
  virtual auto finishedAndNotBusy() -> bool = 0;
};

}  // namespace arangodb::pregel::actor
