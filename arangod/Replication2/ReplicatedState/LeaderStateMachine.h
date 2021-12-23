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
// risk: namespacetangle

namespace arangodb::replication2::replicated_state {

using namespace arangodb::replication2;

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

struct ParticipantHealth {
  RebootId rebootId;
  bool isHealthy;
};

struct ParticipantsHealth {
  auto isHealthy(ParticipantId participant) const -> bool {
    if (auto it = _health.find(participant); it != std::end(_health)) {
      return it->second.isHealthy;
    }
    return false;
  };
  auto validRebootId(ParticipantId participant, RebootId rebootId) const
      -> bool {
    if (auto it = _health.find(participant); it != std::end(_health)) {
      return it->second.rebootId == rebootId;
    }
    return false;
  };

  std::unordered_map<ParticipantId, ParticipantHealth> _health;
};

struct LeaderElectionCampaign {
  enum class Reason { ServerIll, TermNotConfirmed, OK };

  std::unordered_map<ParticipantId, Reason> reasons;
  size_t numberOKParticipants{0};
  replication2::TermIndexPair bestTermIndex;
  std::vector<ParticipantId> electibleLeaderSet;

  void toVelocyPack(VPackBuilder& builder) const;
};
auto to_string(LeaderElectionCampaign::Reason reason) -> std::string_view;
auto operator<<(std::ostream& os, LeaderElectionCampaign::Reason reason) -> std::ostream&;

auto to_string(LeaderElectionCampaign const& campaign) -> std::string;
auto operator<<(std::ostream& os, LeaderElectionCampaign const& action) -> std::ostream&;

struct Action {
  enum class ActionType { UpdateTermAction, SuccessfulLeaderElectionAction, FailedLeaderElectionAction, ImpossibleCampaignAction };
  virtual void execute() = 0;
  virtual ActionType type() const = 0;
  virtual void toVelocyPack(VPackBuilder& builder) const = 0;
  virtual ~Action() = default;
};

auto to_string(Action::ActionType action) -> std::string_view;
auto operator<<(std::ostream& os, Action::ActionType const& action) -> std::ostream&;
auto operator<<(std::ostream& os, Action const& action) -> std::ostream&;

struct UpdateTermAction : Action {
  UpdateTermAction(Log::Plan::TermSpecification const& newTerm)
      : _newTerm(newTerm){};
  void execute() override{};
  ActionType type() const override { return Action::ActionType::UpdateTermAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  Log::Plan::TermSpecification _newTerm;
};

auto to_string(UpdateTermAction action) -> std::string;
auto operator<<(std::ostream& os, UpdateTermAction const& action) -> std::ostream&;

struct SuccessfulLeaderElectionAction : Action {
  SuccessfulLeaderElectionAction(){};
  void execute() override{};
  ActionType type() const override { return Action::ActionType::SuccessfulLeaderElectionAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  LeaderElectionCampaign _campaign;
  ParticipantId _newLeader;
  Log::Plan::TermSpecification _newTerm;
};
auto to_string(SuccessfulLeaderElectionAction action) -> std::string;
auto operator<<(std::ostream& os, SuccessfulLeaderElectionAction const& action) -> std::ostream&;

struct FailedLeaderElectionAction : Action {
  FailedLeaderElectionAction(){};
  void execute() override{};
  ActionType type() const override { return Action::ActionType::FailedLeaderElectionAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  LeaderElectionCampaign _campaign;
};
auto to_string(FailedLeaderElectionAction const& action) -> std::string;
auto operator<<(std::ostream& os, FailedLeaderElectionAction const& action) -> std::ostream&;


struct ImpossibleCampaignAction : Action {
  ImpossibleCampaignAction(){};
  void execute() override{};
  ActionType type() const override { return Action::ActionType::ImpossibleCampaignAction; };
  void toVelocyPack(VPackBuilder& builder) const override;
};
auto to_string(ImpossibleCampaignAction const& action) -> std::string;
auto operator<<(std::ostream& os, ImpossibleCampaignAction const& action) -> std::ostream&;



auto computeReason(Log::Current::LocalState const& status, bool healthy,
                   LogTerm term) -> LeaderElectionCampaign::Reason;

auto runElectionCampaign(Log::Current::LocalStates const& states,
                         ParticipantsHealth const& health, LogTerm term)
    -> LeaderElectionCampaign;

auto replicatedLogAction(Log const&, ParticipantsHealth const&)
    -> std::unique_ptr<Action>;

}  // namespace arangodb::replication2::replicated_state
