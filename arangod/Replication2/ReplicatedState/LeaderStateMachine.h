#pragma once

#include "Basics/ErrorCode.h"

#include "Replication2/ReplicatedLog/LogCommon.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>


// We have
//  - ReplicatedLog
//  - ReplicatedState
//
//  ReplicatedState is built on top of ReplicatedLogs
//
// We have (in the agency)
//  - Current
//  - Plan
//  - Target
//  for each of ReplicatedLog and ReplicatedState
//
//  write down structs that reflect the necessary information
//  in the agency for everything involved here
//
//  Then write (a) function(s)
// auto replicatedStateAction(Log, State) -> Action;

// we use namespacing to separate definitions from implementations as well as
// some of the noise from the essential information.
//
// For instance  this way we can write up the struct for Log:
//
// namespace log {
//    using namespace target;
//    using namespace current;
//    using namespace plan;
//
//    struct Log {
//        Target target;
//        Current current;
//        Plan plan;
//    };
// }
//
// and move the implementations into namespaced sections.
//
// In particular we do not litter struct Log {} with defining sub-structs
// and sub-sub-structs.
//
// risk: spreading the namespace out too much.


namespace arangodb::replication2::replicated_state {

using namespace arangodb::replication2;

using Generation = size_t;

namespace log {
  namespace target {
    struct ParticipantMap {};

    struct Target {
        ParticipantMap participants;
    };
  }

  namespace current {
    struct Current {

    };
  }

  namespace plan {
    struct Plan {

    };
  }
}

namespace log {
    using namespace target;
    using namespace current;
    using namespace plan;

    // goal: make the struct definition as concise as one can
    // ideal example is below: the struct consists of only
    // the relevant members, no syntactic (or otherwise) noise
    struct Log {
        Target target;
        Current current;
        Plan plan;
    };
}


namespace state {
  namespace target {
    namespace properties {

      enum class Hash { Crc32 };
      enum class Implementation { DocumentStore };

      struct Properties {
        Hash hash;
        Implementation implementation;
      };
    }

    namespace participants {
      struct Participant {};
      using Participants = std::unordered_map<ParticipantId, Participant>;
    }

    namespace configuration {
      struct Configuration {
        bool waitForSync;
        size_t writeConcern;
        size_t softWriteConcern;
      };
    }

    using namespace properties;
    using namespace participants;
    using namespace configuration;

    struct Target {
      LogId id;
      Properties properties;
      Participants participants;
      Configuration configuration;
    };
  }

  namespace current {
    struct Current {

    };
  }
  namespace plan {
    namespace participants {
      struct Participant { Generation generation; };
      using Participants = std::unordered_map<ParticipantId, Participant>;
    }

    using namespace participants;
    struct Plan {
      LogId id;
      Generation generation;
      Participants participants;
    };
  }

  using namespace target;
  using namespace current;
  using namespace plan;
  struct State {
    Target target;
    Current current;
    Plan plan;
  };
}



namespace action {
  enum class Action { Update, Delete, DoStuff };
}

auto replicatedStateAction(log::Log, state::State) -> action::Action;

}
