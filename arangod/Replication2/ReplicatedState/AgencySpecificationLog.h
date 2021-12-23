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
/*
 * the following datastructures reflect the layout of the
 * agency with respect to the ReplicatedLog and ReplicatedState
 * subtrees
 */
struct Log {
  struct Target {
    struct Participant {
      bool forced;
    };
    using Participants = std::unordered_map<ParticipantId, Participant>;

    struct Config {
      size_t writeConcern;
      size_t softWriteConcern;
      bool waitForSync;
    };

    using Leader = std::optional<ParticipantId>;

    struct Properties {
      // ...
    };

    LogId id;
    Participants participants;
    Config config;
    Leader leader;
    Properties properties;
  };

  struct Plan {
    struct TermSpecification {
      struct Leader {
        ParticipantId serverId;
        RebootId rebootId;

        void toVelocyPack(VPackBuilder&) const;
      };
      struct Config {
        bool waitForSync;
        size_t writeConcern;
        size_t softWriteConcern;

        void toVelocyPack(VPackBuilder&) const;
      };

      LogTerm term;
      std::optional<Leader> leader;
      Config config;

      void toVelocyPack(VPackBuilder&) const;
    };

    struct Participants {
      using Generation = size_t;

      struct Participant {
        bool forced;
        bool excluded;

        void toVelocyPack(VPackBuilder& builder) const;
      };
      using Set = std::unordered_map<ParticipantId, Participant>;

      Generation generation;
      Set set;
    };

    TermSpecification termSpec;
    Participants participants;
  };

  struct Current {
    struct LocalState {
      LogTerm term;
      TermIndexPair spearhead;
    };
    using LocalStates = std::unordered_map<ParticipantId, LocalState>;

    struct Leader {
      using Term = size_t;

      struct Participants {
        using Generation = size_t;

        Generation generation;
        // TODO: probably missing, participants
      };

      Term term;
      Participants participants;
    };

    struct Supervision {
      // TODO.
    };

    LocalStates localStates;
    Leader leader;
    Supervision supervision;
  };

  Target target;
  Plan plan;
  Current current;
};

};  // namespace arangodb::replication2::replicated_state::agency
