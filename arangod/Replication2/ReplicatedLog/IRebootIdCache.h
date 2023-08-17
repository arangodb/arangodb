////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/RebootId.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

#include <function2.hpp>

#include <unordered_map>
#include <vector>

namespace arangodb {
struct PeerState;
}
namespace arangodb::cluster {
class CallbackGuard;
}

namespace arangodb::replication2::replicated_log {

struct IRebootIdCache {
  using Callback = fu2::unique_function<void()>;

  virtual ~IRebootIdCache() = default;
  virtual auto getRebootIdsFor(std::vector<ParticipantId> const& participants)
      const -> std::unordered_map<ParticipantId, RebootId> = 0;
  virtual auto registerCallbackOnChange(PeerState peer, Callback callback,
                                        std::string description)
      -> cluster::CallbackGuard = 0;

  // convenience method
  auto getRebootIdFor(ParticipantId const& participant) const -> RebootId {
    return getRebootIdsFor({participant}).begin()->second;
  }
};

}  // namespace arangodb::replication2::replicated_log
