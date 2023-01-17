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

#include "ActorPID.h"
#include "Message.h"

namespace arangodb::pregel::actor {

struct ActorBase {
  virtual ~ActorBase() = default;
  virtual auto process(ActorPID sender,
                       std::unique_ptr<MessagePayloadBase> payload) -> void = 0;
  virtual auto process(ActorPID sender, velocypack::SharedSlice msg)
      -> void = 0;
  virtual auto typeName() -> std::string_view = 0;
  virtual auto serialize() -> velocypack::SharedSlice = 0;
};

namespace {
struct ActorInfo {
  ActorID id;
  std::string_view type;
};
template<typename Inspector>
auto inspect(Inspector& f, ActorInfo& x) {
  return f.object(x).fields(f.embedFields(x.id), f.field("type", x.type));
}
}  // namespace

struct ActorMap : std::unordered_map<ActorID, std::unique_ptr<ActorBase>> {};
template<typename Inspector>
auto inspect(Inspector& f, ActorMap& x) {
  if constexpr (Inspector::isLoading) {
    return inspection::Status{};
  } else {
    auto actorInfos = std::vector<ActorInfo>{};
    for (auto const& [id, actor] : x) {
      actorInfos.emplace_back(ActorInfo{.id = id, .type = actor->typeName()});
    }
    return f.apply(actorInfos);
  }
}

};  // namespace arangodb::pregel::actor
