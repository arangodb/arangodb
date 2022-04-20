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
#include "Inspection/VPack.h"

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
  if (supervision.has_value()) {
    builder.add(velocypack::Value("supervision"));
    supervision->toVelocyPack(builder);
  }
}

auto Current::fromVelocyPack(velocypack::Slice slice) -> Current {
  Current c;
  if (auto os = slice.get(StaticStrings::Participants); os.isObject()) {
    for (auto [key, value] :
         velocypack::ObjectIterator(slice.get(StaticStrings::Participants))) {
      auto status = ParticipantStatus::fromVelocyPack(value);
      c.participants.emplace(key.copyString(), std::move(status));
    }
  }
  if (auto ss = slice.get("supervision"); !ss.isNone()) {
    c.supervision = Supervision::fromVelocyPack(ss);
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
  if (version.has_value()) {
    builder.add(StaticStrings::Version, *version);
  }
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
  auto version = std::optional<std::uint64_t>{};
  if (auto vs = slice.get(StaticStrings::Version); !vs.isNone()) {
    version = vs.extract<std::uint64_t>();
  }

  return Target{.id = id,
                .properties = std::move(properties),
                .leader = std::move(leader),
                .participants = std::move(participants),
                .config = config,
                .version = version};
}

void Current::Supervision::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::serialize(builder, *this);
}

auto Current::Supervision::fromVelocyPack(velocypack::Slice slice)
    -> Current::Supervision {
  return velocypack::deserialize<Supervision>(slice);
}

auto StatusCodeStringTransformer::toSerialized(StatusCode source,
                                               std::string& target) const
    -> inspection::Status {
  target = to_string(source);
  return {};
}

auto StatusCodeStringTransformer::fromSerialized(std::string const& source,
                                                 StatusCode& target) const
    -> inspection::Status {
  if (source == "LogNotCreated") {
    target = StatusCode::kLogNotCreated;
  } else if (source == "LogCurrentNotAvailable") {
    target = StatusCode::kLogCurrentNotAvailable;
  } else if (source == "ServerSnapshotMissing") {
    target = StatusCode::kServerSnapshotMissing;
  } else if (source == "InsufficientSnapshotCoverage") {
    target = StatusCode::kInsufficientSnapshotCoverage;
  } else if (source == "LogParticipantNotYetGone") {
    target = StatusCode::kLogParticipantNotYetGone;
  } else {
    inspection::Status{"Invalid status code name " + std::string{source}};
  }
  return {};
}

auto agency::to_string(StatusCode code) noexcept -> std::string_view {
  switch (code) {
    case Current::Supervision::kLogNotCreated:
      return "LogNotCreated";
    case Current::Supervision::kLogCurrentNotAvailable:
      return "LogCurrentNotAvailable";
    case Current::Supervision::kServerSnapshotMissing:
      return "ServerSnapshotMissing";
    case Current::Supervision::kInsufficientSnapshotCoverage:
      return "InsufficientSnapshotCoverage";
    case Current::Supervision::kLogParticipantNotYetGone:
      return "LogParticipantNotYetGone";
    default:
      return "(unknown status code)";
  }
}
