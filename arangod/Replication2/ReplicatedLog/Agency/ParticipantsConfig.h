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
#pragma once

#include <iosfwd>
#include <unordered_map>

#include "Replication2/ReplicatedLog/Agency/LogPlanConfig.h"
#include "Replication2/ReplicatedLog/ParticipantFlags.h"
#include "Replication2/ReplicatedLog/ParticipantId.h"

namespace arangodb::replication2::agency {

using ParticipantsFlagsMap =
    std::unordered_map<ParticipantId, ParticipantFlags>;

struct ParticipantsConfig {
  std::size_t generation = 0;
  ParticipantsFlagsMap participants;
  LogPlanConfig config{};

  friend auto operator==(ParticipantsConfig const& left,
                         ParticipantsConfig const& right) noexcept
      -> bool = default;
  friend auto operator<<(std::ostream& os, ParticipantsConfig const&)
      -> std::ostream&;
};

auto operator<<(std::ostream& os, ParticipantsConfig const&) -> std::ostream&;

template<class Inspector>
auto inspect(Inspector& f, ParticipantsConfig& x) {
  return f.object(x).fields(f.field("generation", x.generation),
                            f.field("config", x.config),
                            f.field("participants", x.participants));
}

}  // namespace arangodb::replication2::agency
