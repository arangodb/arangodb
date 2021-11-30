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
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::agency;

void Current::ParticipantStatus::Snapshot::Error::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  ob->add("error", velocypack::Value(error));
  if (message) {
    ob->add("message", velocypack::Value(*message));
  }
  // TODO timestamp
}

auto Current::ParticipantStatus::Snapshot::Error::fromVelocyPack(velocypack::Slice slice)
    -> Error {
  auto errorCode = ErrorCode{slice.get("error").extract<int>()};
  std::optional<std::string> message;
  if (auto message_slice = slice.get("message"); !message_slice.isNone()) {
    message = message_slice.copyString();
  }
  return Error{errorCode, message, {}};
}

using Status = Current::ParticipantStatus::Snapshot::Status;

auto agency::to_string(Status status) -> std::string_view {
  switch (status) {
    case Status::kInProgress:
      return "InProgress";
    case Status::kCompleted:
      return "Completed";
    case Status::kFailed:
      return "Failed";
    default:
      TRI_ASSERT(false) << "status value was " << static_cast<int>(status);
  }
}

auto agency::from_string(std::string_view string) -> Status {
  if (string == "InProgress") {
    return Status::kInProgress;
  } else if (string == "Completed") {
    return Status::kCompleted;
  } else if (string == "Failed") {
    return Status::kFailed;
  }
  // TODO make a proper error code
  THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
}

void Current::ParticipantStatus::Snapshot::toVelocyPack(velocypack::Builder& builder) const {
  // TODO add timestamp
  VPackObjectBuilder ob(&builder);
  if (error) {
    ob->add(velocypack::Value("error"));
    error->toVelocyPack(*ob);
  }
  ob->add("status", velocypack::Value(to_string(status)));
}

auto Current::ParticipantStatus::Snapshot::fromVelocyPack(velocypack::Slice slice)
    -> Snapshot {

  std::optional<Error> error;
  if (auto error_slice = slice.get("error"); !error_slice.isNone()) {
    error = Error::fromVelocyPack(error_slice);
  }
  Status status = agency::from_string(slice.get("status").stringView());
  return Snapshot{status, {}, std::move(error)};
}
