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
#include "Basics/VelocyPackHelper.h"
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
    return inspection::Status{"Invalid status code name " +
                              std::string{source}};
  }
  return {};
}

auto replicated_state::agency::to_string(StatusCode code) noexcept
    -> std::string_view {
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

auto replicated_state::agency::operator==(ImplementationSpec const& s,
                                          ImplementationSpec const& s2) noexcept
    -> bool {
  if (s.type != s2.type ||
      s.parameters.has_value() != s2.parameters.has_value()) {
    return false;
  }
  return !s.parameters.has_value() ||
         basics::VelocyPackHelper::equal(s.parameters->slice(),
                                         s2.parameters->slice(), true);
}
