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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ErrorCode.h"
#include "Cluster/ClusterTypes.h"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

#include "Replication2/ReplicatedLog/LogCommon.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace arangodb::replication2::replicated_state::agency {
struct State {
  struct Target {
    using StateId = size_t;
    struct Properties {
      enum class Hash { Crc32 };
      enum class Implementation { DocumentStore };

      Hash hash;
      Implementation implementation;
    };
    struct Configuration {
      bool waitForSync;
      size_t writeConcern;
      size_t softWriteConcern;
    };
    struct Participant {
      // TODO
    };
    using Participants = std::unordered_map<ParticipantId, Participant>;

    StateId id;
    Properties properties;
    Configuration configuration;
    Participants participants;
  };
  struct Plan {
    using PlanId = size_t;
    using Generation = size_t;
    struct Participant {
      Generation generation;
    };
    using Participants = std::unordered_map<ParticipantId, Participant>;

    PlanId id;
    Generation generation;
    Participants participants;
  };
  struct Current {
    using CurrentId = size_t;

    struct Participant {
      using Generation = size_t;
      struct Snapshot {
        enum class Status { Completed, InProgreess, Failed };
        struct Timestamp {
          // TODO std::chrono?
        };

        Status status;
        Timestamp timestamp;
      };

      Generation generation;
      Snapshot snapshot;
    };
    using Participants = std::unordered_map<ParticipantId, Participant>;

    CurrentId id;
    Participants participants;
  };

  Target target;
  Plan plan;
  Current current;
};
};  // namespace arangodb::replication2::replicated_state::agency
