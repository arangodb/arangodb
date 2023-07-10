////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "LogMetaPayload.h"

#include "Inspection/VPack.h"

namespace arangodb::replication2 {

auto LogMetaPayload::fromVelocyPack(velocypack::Slice s) -> LogMetaPayload {
  return velocypack::deserialize<LogMetaPayload>(s);
}

void LogMetaPayload::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::serialize(builder, *this);
}

auto LogMetaPayload::withFirstEntryOfTerm(ParticipantId leader,
                                          agency::ParticipantsConfig config)
    -> LogMetaPayload {
  return LogMetaPayload{FirstEntryOfTerm{.leader = std::move(leader),
                                         .participants = std::move(config)}};
}

auto LogMetaPayload::withUpdateInnerTermConfig(
    agency::ParticipantsConfig config,
    std::unordered_map<ParticipantId, RebootId> safeRebootIds)
    -> LogMetaPayload {
  return LogMetaPayload{
      UpdateInnerTermConfig{.participants = std::move(config),
                            .safeRebootIds = std::move(safeRebootIds)}};
}

auto LogMetaPayload::withPing(std::optional<std::string> message,
                              Ping::clock::time_point tp) noexcept
    -> LogMetaPayload {
  return LogMetaPayload{Ping{std::move(message), tp}};
}

}  // namespace arangodb::replication2
