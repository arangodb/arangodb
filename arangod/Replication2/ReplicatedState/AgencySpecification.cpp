////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::agency;

namespace {
auto const StringMessage = velocypack::StringRef{"message"};
auto const String_Status = velocypack::StringRef{"status"};
auto const String_Snapshot = velocypack::StringRef{"snapshot"};
auto const String_Generation = velocypack::StringRef{"generation"};
auto const String_InProgress = std::string_view{"InProgress"};
auto const String_Completed = std::string_view{"Completed"};
auto const String_Failed = std::string_view{"Failed"};
}  // namespace

void Current::ParticipantStatus::Snapshot::Error::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  ob->add(StaticStrings::Error, velocypack::Value(error));
  ob->add(StaticStrings::ErrorMessage,
          velocypack::Value(TRI_errno_string(error)));
  if (message) {
    ob->add(StringMessage, velocypack::Value(*message));
  }
  // TODO timestamp
}

auto Current::ParticipantStatus::Snapshot::Error::fromVelocyPack(
    velocypack::Slice slice) -> Error {
  auto errorCode = ErrorCode{slice.get(StaticStrings::Error).extract<int>()};
  std::optional<std::string> message;
  if (auto message_slice = slice.get(StringMessage); !message_slice.isNone()) {
    message = message_slice.copyString();
  }
  return Error{errorCode, message, {}};
}

using Status = Current::ParticipantStatus::Snapshot::Status;

auto agency::to_string(Status status) -> std::string_view {
  switch (status) {
    case Status::kInProgress:
      return String_InProgress;
    case Status::kCompleted:
      return String_Completed;
    case Status::kFailed:
      return String_Failed;
  }
  TRI_ASSERT(false) << "status value was " << static_cast<int>(status);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
}

auto agency::from_string(std::string_view string) -> Status {
  if (string == String_InProgress) {
    return Status::kInProgress;
  } else if (string == String_Completed) {
    return Status::kCompleted;
  } else if (string == String_Failed) {
    return Status::kFailed;
  }
  // TODO make a proper error code
  THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
}

void Current::ParticipantStatus::Snapshot::toVelocyPack(
    velocypack::Builder& builder) const {
  // TODO add timestamp
  VPackObjectBuilder ob(&builder);
  if (error) {
    ob->add(velocypack::Value(StaticStrings::Error));
    error->toVelocyPack(*ob);
  }
  ob->add(String_Status, velocypack::Value(to_string(status)));
}

auto Current::ParticipantStatus::Snapshot::fromVelocyPack(
    velocypack::Slice slice) -> Snapshot {
  std::optional<Error> error;
  if (auto error_slice = slice.get(StaticStrings::Error);
      !error_slice.isNone()) {
    error = Error::fromVelocyPack(error_slice);
  }
  Status status = agency::from_string(slice.get(String_Status).stringView());
  return Snapshot{.status = status, .timestamp = {}, .error = std::move(error)};
}

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
  auto snapshot = Snapshot::fromVelocyPack(slice.get(String_Snapshot));
  return ParticipantStatus{.generation = generation,
                           .snapshot = std::move(snapshot)};
}

void Current::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Id, velocypack::Value(id));
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
  c.id = slice.get(StaticStrings::Id).extract<LogId>();
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

  return Plan{.id = id,
              .generation = generation,
              .participants = std::move(participants)};
}
