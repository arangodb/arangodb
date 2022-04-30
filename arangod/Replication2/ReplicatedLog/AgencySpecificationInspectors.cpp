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
////////////////////////////////////////////////////////////////////////////////

#include "AgencySpecificationInspectors.h"

namespace arangodb::replication2::agency {

auto to_string(StatusCode code) -> std::string_view {
  switch (code) {
    case StatusCode::kTargetLeaderExcluded:
      return "TargetLeaderExcluded";
    case StatusCode::kTargetLeaderInvalid:
      return "TargetLeaderInvalid";
    case StatusCode::kTargetNotEnoughParticipants:
      return "NotEnoughParticipants";
    case StatusCode::kWaitingForConfigCommitted:
      return "WaitingForConfigCommitted";
    case StatusCode::kConfigChangeNotImplemented:
      return "ConfigChangeNotImplemented";
    case StatusCode::kLeaderElectionImpossible:
      return "LeaderElectionImpossible";
    case StatusCode::kTargetLeaderFailed:
      return "TargetLeaderFailed";
    case StatusCode::kDictateLeaderFailed:
      return "DictateLeaderFailed";
    case StatusCode::kPlanNotAvailable:
      return "PlanNotAvailable";
    case StatusCode::kCurrentNotAvailable:
      return "CurrentNotAvailable";
    default:
      return "(unknown status code)";
  }
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
  if (source == "TargetLeaderExcluded") {
    target = StatusCode::kTargetLeaderExcluded;
  } else if (source == "TargetLeaderInvalid") {
    target = StatusCode::kTargetLeaderInvalid;
  } else if (source == "NotEnoughParticipants") {
    target = StatusCode::kTargetNotEnoughParticipants;
  } else if (source == "WaitingForConfigSubmitted") {
    target = StatusCode::kWaitingForConfigCommitted;
  } else if (source == "ConfigChangeNotImplemented") {
    target = StatusCode::kConfigChangeNotImplemented;
  } else if (source == "LeaderElectionImpossible") {
    target = StatusCode::kLeaderElectionImpossible;
  } else if (source == "TargetLeaderFailed") {
    target = StatusCode::kTargetLeaderFailed;
  } else if (source == "DictateLeaderFailed") {
    target = StatusCode::kDictateLeaderFailed;
  } else if (source == "PlanNotAvailable") {
    target = StatusCode::kPlanNotAvailable;
  } else if (source == "CurrentNotAvailable") {
    target = StatusCode::kCurrentNotAvailable;
  } else {
    inspection::Status{"Invalid status code name " + std::string{source}};
  }
  return {};
}

}  // namespace arangodb::replication2::agency
