////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Agency/AgencyCommon.h"
#include "Agency/Store.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Cluster/ClusterTypes.h"

#include "Metrics/Fwd.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace arangodb {
namespace velocypack {
class Slice;
}
namespace replication2::replicated_log {
struct ParticipantsHealth;
}

namespace consensus {

class Agent;
class AgentInterface;

struct check_t {
  bool good;
  std::string name;
  check_t(std::string const& n, bool g) : good(g), name(n) {}
};

// This is the functional version which actually does the work, it is
// called by the private method Supervision::enforceReplication and the
// unit tests:
void enforceReplicationFunctional(Node const& snapshot, uint64_t& jobId,
                                  std::shared_ptr<VPackBuilder> envelope,
                                  uint64_t delayAddFollower = 0);

// This is the functional version which actually does the work, it is
// called by the private method Supervision::cleanupHotbackupTransferJobs
// and the unit tests:
void cleanupHotbackupTransferJobsFunctional(
    Node const& snapshot, std::shared_ptr<VPackBuilder> envelope);

// This is the second functional version which actually does the work, it is
// called by the private method Supervision::cleanupHotbackupTransferJobs
// and the unit tests:
void failBrokenHotbackupTransferJobsFunctional(
    Node const& snapshot, std::shared_ptr<VPackBuilder> envelope);

class Supervision : public ServerThread<ArangodServer> {
 public:
  typedef std::chrono::system_clock::time_point TimePoint;
  typedef std::string ServerID;

  /// @brief Construct cluster consistency checking
  Supervision(ArangodServer& server, metrics::MetricsFeature& metrics);

  /// @brief Default dtor
  ~Supervision();

  /// @brief Start thread
  bool start();

  /// @brief Start thread with access to agent
  bool start(Agent*);

  /// @brief Run woker
  void run() override final;

  /// @brief Begin thread shutdown
  void beginShutdown() override final;

  /// @brief Upgrade agency
  void upgradeAgency();

  /// @brief Upgrade hot backup entry, if 0
  void upgradeMaintenance(VPackBuilder& builder);

  /// @brief Upgrade maintenance key, if "on"
  void upgradeBackupKey(VPackBuilder& builder);

  /// @brief remove hotbackup lock in agency, if expired
  void unlockHotBackup();

  /**
   * @brief Helper function to build transaction removing no longer
   *        present servers from health monitoring
   *
   * @param  del       Agency transaction builder
   * @param  todelete  List of servers to be removed
   */
  static void buildRemoveTransaction(velocypack::Builder& del,
                                     std::vector<std::string> const& todelete);

  static constexpr std::string_view HEALTH_STATUS_GOOD = "GOOD";
  static constexpr std::string_view HEALTH_STATUS_BAD = "BAD";
  static constexpr std::string_view HEALTH_STATUS_FAILED = "FAILED";
  // should never be stored in the agency. only used internally to return an
  // unclear health status
  static constexpr std::string_view HEALTH_STATUS_UNCLEAR = "UNCLEAR";

  static std::string agencyPrefix() { return _agencyPrefix; }

  static void setAgencyPrefix(std::string const& prefix) {
    _agencyPrefix = prefix;
  }

  static std::string serverHealthFunctional(Node const& snapshot,
                                            std::string_view);

  static bool verifyServerRebootID(Node const& snapshot,
                                   std::string const& serverID,
                                   uint64_t wantedRebootID, bool& serverFound);

  // public only for unit testing:
  static void cleanupLostCollections(Node const& snapshot,
                                     AgentInterface* agent, uint64_t& jobId);

  // public only for unit testing:
  static void deleteBrokenDatabase(AgentInterface* agent,
                                   std::string const& database,
                                   std::string const& coordinatorID,
                                   uint64_t rebootID, bool coordinatorFound);

  // public only for unit testing:
  static void deleteBrokenCollection(AgentInterface* agent,
                                     std::string const& database,
                                     std::string const& collection,
                                     std::string const& coordinatorID,
                                     uint64_t rebootID, bool coordinatorFound);

  // public only for unit testing:
  static void deleteBrokenIndex(AgentInterface* agent,
                                std::string const& database,
                                std::string const& collection,
                                arangodb::velocypack::Slice index,
                                std::string const& coordinatorID,
                                uint64_t rebootID, bool coordinatorFound);

  void setOkThreshold(double d) noexcept { _okThreshold = d; }

  void setGracePeriod(double d) noexcept { _gracePeriod = d; }

  void setDelayAddFollower(uint64_t d) noexcept { _delayAddFollower = d; }

  void setDelayFailedFollower(uint64_t d) noexcept { _delayFailedFollower = d; }

  void setFailedLeaderAddsFollower(bool b) noexcept {
    _failedLeaderAddsFollower = b;
  }

  /// @brief notifies the supervision and triggers a new run
  void notify() noexcept;

 private:
  /// @brief wait for the supervision node to appear (cluster bootstrap)
  void waitForSupervisionNode();

  /// @brief does one round of supervision business
  void step();

  /// @brief waits for the given index to be committed
  void waitForIndexCommitted(index_t);

  /// @brief get reference to the spearhead snapshot
  Node const& snapshot() const;

  /// @brief decide, if we can start supervision ahead of armageddon delay
  bool earlyBird() const;

  /// @brief Upgrade agency with FailedServers an object from array
  void upgradeZero(VPackBuilder&);

  /// @brief Upgrade agency to supervision overhaul jobs
  void upgradeOne(VPackBuilder&);

  /// @brief Upgrade agency to supervision overhaul jobs
  void upgradeHealthRecords(VPackBuilder&);

  /// @brief Check undo-leader-change-actions
  void checkUndoLeaderChangeActions();

  /// @brief Check for orphaned index creations, which have been successfully
  /// built
  void readyOrphanedIndexCreations();

  /// @brief Check for orphaned index creations, which have been successfully
  /// built
  void checkBrokenCreatedDatabases();

  /// @brief Check for boken collections
  void checkBrokenCollections();

  /// @brief Check for broken analyzers
  void checkBrokenAnalyzers();

  /// @brief Check replicated logs
  void checkReplicatedLogs();

  /// @brief Check collection groups
  void checkCollectionGroups();

  /// @brief Clean up replicated logs
  void cleanupReplicatedLogs();

  struct ResourceCreatorLostEvent {
    std::shared_ptr<Node const> const& resource;
    std::string const& coordinatorId;
    uint64_t coordinatorRebootId;
    bool coordinatorFound;
  };

  // @brief Checks if a resource (database or collection). Action is called if
  // resource should be deleted
  void ifResourceCreatorLost(
      std::shared_ptr<Node const> const& resource,
      std::function<void(ResourceCreatorLostEvent const&)> const& action);

  // @brief Action is called if resource should be deleted
  void resourceCreatorLost(
      std::shared_ptr<Node const> const& resource,
      std::function<void(ResourceCreatorLostEvent const&)> const& action);

  /// @brief Check for inconsistencies in replication factor vs dbs entries
  void enforceReplication();

  /// @brief Check machines in agency
  std::vector<check_t> check(std::string const&);

  /// @brief Cleanup old Supervision jobs
  void cleanupFinishedAndFailedJobs();

  /// @brief Cleanup old hotbackup transfer jobs
  void cleanupHotbackupTransferJobs();

  /// @brief Fail hotbackup transfer jobs when dbservers have failed
  void failBrokenHotbackupTransferJobs();

  // @brief these servers have gone for too long without any responsibility
  //        and this are safely removable and so they are
  void cleanupExpiredServers(Node const&, Node const&);

  void workJobs();

  /// @brief Get unique ids from agency
  void getUniqueIds();

  /// @brief Perform consistency checking
  bool doChecks();

  /// @brief update my local agency snapshot
  bool updateSnapshot();

  void shrinkCluster();

  /**
   * @brief Report status of supervision in agency
   * @param  status  Status, which will show in Supervision/State
   */
  void reportStatus(std::string const& status);

  void updateDBServerMaintenance();

  replication2::replicated_log::ParticipantsHealth collectParticipantsHealth()
      const;

  void handleJobs();

  void restoreBrokenAnalyzersRevision(
      std::string const& database, AnalyzersRevision::Revision revision,
      AnalyzersRevision::Revision buildingRevision,
      std::string const& coordinatorID, uint64_t rebootID,
      bool coordinatorFound);

  std::mutex _lock;  // guards snapshot, _jobId, jobIdMax, _selfShutdown
  Agent* _agent;     /**< @brief My agent */
  mutable std::shared_ptr<Node const> _snapshot;
  std::shared_ptr<Node const> _transient;

  arangodb::basics::ConditionVariable _cv; /**< @brief Control if thread
                                              should run */

  // The following variables can be set from the outside during runtime
  // and can create data races between the rest handler and the supervision
  // thread.
  std::atomic<double> _frequency;
  std::atomic<double> _gracePeriod;
  std::atomic<double> _okThreshold;
  std::atomic<uint64_t> _delayAddFollower;
  std::atomic<uint64_t> _delayFailedFollower;
  std::atomic<bool> _failedLeaderAddsFollower;
  uint64_t _jobId;
  uint64_t _jobIdMax;
  uint64_t _lastUpdateIndex;
  bool _shouldRunAgain = false;

  bool _haveAborts; /**< @brief We have accumulated pending aborts in a round */

  std::atomic<bool> _upgraded;
  std::chrono::system_clock::time_point _nextServerCleanup;

  std::string serverHealth(std::string_view) const;

  static std::string _agencyPrefix;  // initialized in AgencyFeature

  // Updated before each supervision run in `updateDBServerMaintenance`:
  std::unordered_set<std::string> _DBServersInMaintenance;

 public:
  metrics::Histogram<metrics::LogScale<uint64_t>>& _supervision_runtime_msec;
  metrics::Histogram<metrics::LogScale<uint64_t>>&
      _supervision_runtime_wait_for_sync_msec;
  metrics::Counter& _supervision_failed_server_counter;
};

}  // namespace consensus
}  // namespace arangodb
