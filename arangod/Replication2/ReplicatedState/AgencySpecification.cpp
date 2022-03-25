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
/// @author Alex Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "AgencySpecification.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::agency;

namespace {
auto const String_Snapshot = velocypack::StringRef{"snapshot"};
auto const String_Generation = velocypack::StringRef{"generation"};

}  // namespace

void Current::ParticipantStatus::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(String_Generation, velocypack::Value(generation));
  builder.add(velocypack::Value(String_Snapshot));
  snapshot.toVelocyPack(builder);
}

auto Current::ParticipantStatus::fromVelocyPack(velocypack::Slice slice)
    -> ParticipantStatus {
  auto generation = slice.get(String_Generation).extract<StateGeneration>();
  auto snapshot = SnapshotInfo::fromVelocyPack(slice.get(String_Snapshot));
  return ParticipantStatus{.generation = generation,
                           .snapshot = std::move(snapshot)};
}

void Current::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  {
    VPackObjectBuilder ab(&builder, StaticStrings::Participants);
    for (auto const& [pid, status] : participants) {
      builder.add(velocypack::Value(pid));
      status.toVelocyPack(builder);
    }
  }
}

auto Current::fromVelocyPack(velocypack::Slice slice) -> Current {
  Current c;
  for (auto [key, value] :
       velocypack::ObjectIterator(slice.get(StaticStrings::Participants))) {
    auto status = ParticipantStatus::fromVelocyPack(value);
    c.participants.emplace(key.copyString(), std::move(status));
  }
  return c;
}

auto Plan::Participant::fromVelocyPack(velocypack::Slice slice) -> Participant {
  return Participant{
      .generation = slice.get(String_Generation).extract<StateGeneration>()};
}

void Plan::Participant::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(String_Generation, velocypack::Value(generation));
}

void Plan::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(StaticStrings::Id, velocypack::Value(id));
  builder.add(String_Generation, velocypack::Value(generation));
  {
    velocypack::ObjectBuilder pob(&builder, StaticStrings::Participants);
    for (auto const& [pid, state] : participants) {
      builder.add(velocypack::Value(pid));
      state.toVelocyPack(builder);
    }
  }
  builder.add(velocypack::Value(StaticStrings::Properties));
  properties.toVelocyPack(builder);
}

auto Plan::fromVelocyPack(velocypack::Slice slice) -> Plan {
  auto id = slice.get(StaticStrings::Id).extract<LogId>();
  auto generation = slice.get(String_Generation).extract<StateGeneration>();
  auto participants = std::unordered_map<ParticipantId, Participant>{};
  for (auto [key, value] :
       velocypack::ObjectIterator(slice.get(StaticStrings::Participants))) {
    auto status = Participant::fromVelocyPack(value);
    participants.emplace(key.copyString(), status);
  }

  auto properties =
      Properties::fromVelocyPack(slice.get(StaticStrings::Properties));

  return Plan{.id = id,
              .generation = generation,
              .properties = std::move(properties),
              .participants = std::move(participants)};
}

void Properties::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(velocypack::Value("implementation"));
  implementation.toVelocyPack(builder);
}

auto Properties::fromVelocyPack(velocypack::Slice slice) -> Properties {
  auto impl = ImplementationSpec::fromVelocyPack(slice.get("implementation"));
  return Properties{.implementation = std::move(impl)};
}

void ImplementationSpec::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(StaticStrings::IndexType, velocypack::Value(type));
}

auto ImplementationSpec::fromVelocyPack(velocypack::Slice slice)
    -> ImplementationSpec {
  return ImplementationSpec{
      .type = slice.get(StaticStrings::IndexType).copyString()};
}

auto Target::Participant::fromVelocyPack(velocypack::Slice slice)
    -> Participant {
  return Participant{};
}

void Target::Participant::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
}

void Target::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(StaticStrings::Id, velocypack::Value(id));
  {
    velocypack::ObjectBuilder pob(&builder, StaticStrings::Participants);
    for (auto const& [pid, state] : participants) {
      builder.add(velocypack::Value(pid));
      state.toVelocyPack(builder);
    }
  }
  if (leader) {
    builder.add(StaticStrings::Leader, *leader);
  }
  builder.add(velocypack::Value(StaticStrings::Properties));
  properties.toVelocyPack(builder);
  builder.add(velocypack::Value(StaticStrings::Config));
  config.toVelocyPack(builder);
}

auto Target::fromVelocyPack(velocypack::Slice slice) -> Target {
  auto id = slice.get(StaticStrings::Id).extract<LogId>();
  auto participants = std::unordered_map<ParticipantId, Participant>{};
  if (auto pslice = slice.get(StaticStrings::Participants); !pslice.isNone()) {
    for (auto [key, value] : velocypack::ObjectIterator(pslice)) {
      auto participant = Participant::fromVelocyPack(value);
      participants.emplace(key.copyString(), participant);
    }
  }
  auto leader = std::invoke([&]() -> decltype(Target::leader) {
    if (auto leaderSlice = slice.get(StaticStrings::Leader);
        !leaderSlice.isNone()) {
      TRI_ASSERT(leaderSlice.isString());
      return {leaderSlice.copyString()};
    } else {
      return std::nullopt;
    }
  });
  auto properties =
      Properties::fromVelocyPack(slice.get(StaticStrings::Properties));

  auto config = LogConfig(slice.get(StaticStrings::Config));

  return Target{.id = id,
                .properties = std::move(properties),
                .leader = std::move(leader),
                .participants = std::move(participants),
                .config = std::move(config)};
}
