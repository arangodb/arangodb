////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CONSENSUS_SUPERVISION_H
#define ARANGOD_CONSENSUS_SUPERVISION_H 1

#include "Agency/AgencyCommon.h"
#include "Agency/AgentInterface.h"
#include "Agency/Store.h"
#include "Agency/TimeString.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Cluster/CriticalThread.h"
#include "RestServer/MetricsFeature.h"

namespace arangodb {
namespace consensus {

class Agent;

struct check_t {
  bool good;
  std::string name;
  check_t(std::string const& n, bool g) : good(g), name(n) {}
};

class Supervision : public arangodb::CriticalThread {
 public:
  typedef std::chrono::system_clock::time_point TimePoint;
  typedef std::string ServerID;
  typedef std::string ServerStatus;
  typedef std::string ServerTimestamp;

  enum TASKS {
    LEADER_FAILURE_MIGRATION,
    FOLLOWER_FAILURE_MIGRATION,
    LEADER_INTENDED_MIGRATION,
    FOLLOWER_INTENDED_MIGRATION
  };

  template <TASKS T>
  class Task {
    explicit Task(const VPackSlice& config) {}
    ServerID _serverID;
    std::string _endpoint;
  };

  struct VitalSign {
    VitalSign(ServerStatus const& s, ServerTimestamp const& t)
        : myTimestamp(std::chrono::system_clock::now()),
          serverStatus(s),
          serverTimestamp(t),
          jobId("0") {}

    void update(ServerStatus const& s, ServerTimestamp const& t) {
      myTimestamp = std::chrono::system_clock::now();
      serverStatus = s;
      serverTimestamp = t;
      jobId = "0";
    }

    void maintenance(std::string const& jid) { jobId = jid; }

    std::string const& maintenance() const { return jobId; }

    TimePoint myTimestamp;
    ServerStatus serverStatus;
    ServerTimestamp serverTimestamp;
    std::string jobId;
  };

  /// @brief Construct cluster consistency checking
  explicit Supervision(application_features::ApplicationServer& server);

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

  static constexpr char const* HEALTH_STATUS_GOOD = "GOOD";
  static constexpr char const* HEALTH_STATUS_BAD = "BAD";
  static constexpr char const* HEALTH_STATUS_FAILED = "FAILED";

  static std::string agencyPrefix() { return _agencyPrefix; }

  static void setAgencyPrefix(std::string const& prefix) {
    _agencyPrefix = prefix;
  }

 private:

  /// @brief get reference to the spearhead snapshot
  Node const& snapshot() const ;

  /// @brief decide, if we can start supervision ahead of armageddon delay
  bool earlyBird() const;

  /// @brief Upgrade agency with FailedServers an object from array
  void upgradeZero(VPackBuilder&);

  /// @brief Upgrade agency to supervision overhaul jobs
  void upgradeOne(VPackBuilder&);

  /// @brief Upgrade agency to supervision overhaul jobs
  void upgradeHealthRecords(VPackBuilder&);

  /// @brief Check for orphaned index creations, which have been successfully built
  void readyOrphanedIndexCreations();

  /// @brief Check for orphaned index creations, which have been successfully built
  void checkBrokenCreatedDatabases();

  /// @brief Check for boken collections
  void checkBrokenCollections();

  /// @brief Check for broken analyzers
  void checkBrokenAnalyzers();

  struct ResourceCreatorLostEvent {
    std::shared_ptr<Node> const& resource;
    std::string const& coordinatorId;
    uint64_t coordinatorRebootId;
    bool coordinatorFound;
  };

  // @brief Checks if a resource (database or collection). Action is called if resource should be deleted
  void ifResourceCreatorLost(std::shared_ptr<Node> const& resource,
                             std::function<void(ResourceCreatorLostEvent const&)> const& action);

  // @brief Action is called if resource should be deleted
  void resourceCreatorLost(std::shared_ptr<Node> const& resource,
                           std::function<void(const ResourceCreatorLostEvent&)> const& action);

  /// @brief Check for inconsistencies in replication factor vs dbs entries
  void enforceReplication();

  /// @brief Move shard from one db server to other db server
  bool moveShard(std::string const& from, std::string const& to);

  /// @brief Move shard from one db server to other db server
  bool replicateShard(std::string const& to);

  /// @brief Move shard from one db server to other db server
  bool removeShard(std::string const& from);

  /// @brief Check machines in agency
  std::vector<check_t> check(std::string const&);

  // @brief Check shards in agency
  std::vector<check_t> checkShards();

  // @brief
  void cleanupFinishedAndFailedJobs();

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

 public:
  static void cleanupLostCollections(Node const& snapshot, AgentInterface* agent,
                                     uint64_t& jobId);

  void setOkThreshold(double d) {
    _okThreshold = d;
  }

  void setGracePeriod(double d) {
    _gracePeriod = d;
  }

 private:
  /**
   * @brief Report status of supervision in agency
   * @param  status  Status, which will show in Supervision/State
   */
  void reportStatus(std::string const& status);

  bool isShuttingDown();

  bool handleJobs();
  void handleShutdown();
  bool verifyCoordinatorRebootID(std::string const& coordinatorID,
                                 uint64_t wantedRebootID, bool& coordinatorFound);
  void deleteBrokenDatabase(std::string const& database, std::string const& coordinatorID,
                            uint64_t rebootID, bool coordinatorFound);
  void deleteBrokenCollection(std::string const& database, std::string const& collection,
                              std::string const& coordinatorID,
                              uint64_t rebootID, bool coordinatorFound);

  void restoreBrokenAnalyzersRevision(std::string const& database,
                                      AnalyzersRevision::Revision revision,
                                      AnalyzersRevision::Revision buildingRevision,
                                      std::string const& coordinatorID,
                                      uint64_t rebootID,
                                      bool coordinatorFound);

  /// @brief Migrate chains of distributeShardsLike to depth 1
  void fixPrototypeChain(VPackBuilder&);

  Mutex _lock;   // guards snapshot, _jobId, jobIdMax, _selfShutdown
  Agent* _agent; /**< @brief My agent */
  Store _spearhead;
  mutable Node const* _snapshot;
  Node _transient;

  arangodb::basics::ConditionVariable _cv; /**< @brief Control if thread
                                              should run */

  double _frequency;
  double _gracePeriod;
  double _okThreshold;
  uint64_t _jobId;
  uint64_t _jobIdMax;
  uint64_t _lastUpdateIndex;

  bool _haveAborts;        /**< @brief We have accumulated pending aborts in a round */

  // mop: this feels very hacky...we have a hen and egg problem here
  // we are using /Shutdown in the agency to determine that the cluster should
  // shutdown. When every member is down we should of course not persist this
  // flag so we don't immediately initiate shutdown after restart. we use this
  // flag to temporarily store that shutdown was initiated...when the /Shutdown
  // stuff has been removed we shutdown ourselves. The assumption (heheh...) is
  // that while the cluster is shutting down every agent hit the shutdown stuff
  // at least once so this flag got set at some point
  bool _selfShutdown;

  std::atomic<bool> _upgraded;
  std::chrono::system_clock::time_point _nextServerCleanup;

  std::string serverHealth(std::string const&);

  static std::string _agencyPrefix;  // initialized in AgencyFeature

 public:
  Histogram<log_scale_t<uint64_t>>& _supervision_runtime_msec;
  Histogram<log_scale_t<uint64_t>>& _supervision_runtime_wait_for_sync_msec;
  Counter& _supervision_accum_runtime_msec;
  Counter& _supervision_accum_runtime_wait_for_sync_msec;
  Counter& _supervision_failed_server_counter;
};

/**
 * @brief Helper function to build transaction removing no longer
 *        present servers from health monitoring
 *
 * @param  todelete  List of servers to be removed
 * @return           Agency transaction
 */
query_t removeTransactionBuilder(std::vector<std::string> const&);

}  // namespace consensus
}  // namespace arangodb

#endif
