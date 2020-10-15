////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_MAINTENANCE_FEATURE
#define ARANGOD_CLUSTER_MAINTENANCE_FEATURE 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Cluster/Action.h"
#include "Cluster/MaintenanceWorker.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/MetricsFeature.h"

#include <memory>
#include <mutex>
#include <queue>

namespace arangodb {

template <typename T>
struct SharedPtrComparer {
  bool operator()(std::shared_ptr<T> const& a, std::shared_ptr<T> const& b) {
    if (a == nullptr || b == nullptr) {
      return false;
    }
    return *a < *b;
  }
};

class MaintenanceFeature : public application_features::ApplicationFeature {
 public:
  explicit MaintenanceFeature(application_features::ApplicationServer&);

  virtual ~MaintenanceFeature() = default;

  struct errors_t {
    std::map<std::string, std::map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>>> indexes;

    // dbname/collection/shardid -> error
    std::unordered_map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>> shards;

    // dbname -> error
    std::unordered_map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>> databases;
  };

  typedef std::map<ShardID, std::shared_ptr<maintenance::ActionDescription>> ShardActionMap;

  /// @brief Lowest limit for worker threads
  static constexpr uint32_t const minThreadLimit = 2;

  /// @brief Highest limit for worker threads
  static constexpr uint32_t const maxThreadLimit = 64;

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  // @brief #databases last time we checked allDatabases
  size_t lastNumberOfDatabases() const;

  // Is maintenance paused?
  bool isPaused() const;

  // Pause maintenance for
  void pause(std::chrono::seconds const& s = std::chrono::seconds(10));

  // Proceed doing maintenance
  void proceed();

  // start the feature
  virtual void start() override;

  // notify the feature about a shutdown request
  virtual void beginShutdown() override;

  // stop the feature
  virtual void stop() override;

  //
  // api features
  //

  /// @brief This is the  API for creating an Action and executing it.
  ///  Execution can be immediate by calling thread, or asynchronous via thread
  ///  pool. not yet:  ActionDescription parameter will be MOVED to new object.
  virtual Result addAction(std::shared_ptr<maintenance::ActionDescription> const& description,
                           bool executeNow = false);

  /// @brief This is the  API for creating an Action and executing it.
  ///  Execution can be immediate by calling thread, or asynchronous via thread
  ///  pool. not yet:  ActionDescription parameter will be MOVED to new object.
  virtual Result addAction(std::shared_ptr<maintenance::Action> action,
                           bool executeNow = false);

  /// @brief Internal API that allows existing actions to create pre actions
  /// FIXDOC: Please explain how this works in a lot more detail, for example,
  /// say if this can be called in the code of an Action and if the already
  /// running action is postponed in this case. Explain semantics such that
  /// somebody not knowing the code can use it.
  std::shared_ptr<maintenance::Action> preAction(
      std::shared_ptr<maintenance::ActionDescription> const& description);

  /// @brief Internal API that allows existing actions to create post actions
  /// FIXDOC: Please explain how this works in a lot more detail, such that
  /// somebody not knowing the code can use it.
  std::shared_ptr<maintenance::Action> postAction(
      std::shared_ptr<maintenance::ActionDescription> const& description);

  /// @brief Check if a shard is locked for a maintenance action.
  /// returns the ActionDescription of the job if locked. If the shard
  /// is not locked, a nullptr is returned.
  std::shared_ptr<maintenance::ActionDescription> isShardLocked(ShardID const& shardId) const;

  /// @brief Lock a shard for a certain action description. Returns `false` if
  /// the shard is already locked and `true` otherwise. If the lock succeeds, the
  /// action description is retained for later query.
  bool lockShard(ShardID const& shardId, std::shared_ptr<maintenance::ActionDescription> const& description);

  /// @brief Release shard lock. Returns `true` if the shard was locked and `false` otherwise.
  bool unlockShard(ShardID const& shardId);

  /// @brief Get shard locks, this copies the whole map of shard locks.
  ShardActionMap getShardLocks() const;

  /// @brief check if a database is dirty
  bool isDirty(std::string const& dbName) const;

 protected:
  std::shared_ptr<maintenance::Action> createAction(
      std::shared_ptr<maintenance::ActionDescription> const& description);

  void registerAction(std::shared_ptr<maintenance::Action> action, bool executeNow);

  std::shared_ptr<maintenance::Action> createAndRegisterAction(
      std::shared_ptr<maintenance::ActionDescription> const& description, bool executeNow);

 public:
  /// @brief This API will attempt to fail an existing Action that is waiting
  ///  or executing.  Will not fail Actions that have already succeeded or failed.
  Result deleteAction(uint64_t id);

  /// @brief Create a VPackBuilder object with snapshot of current action registry
  VPackBuilder toVelocyPack() const;

  /// @brief Fill the envelope with snapshot of current action registry
  void toVelocyPack(VPackBuilder& envelope) const;

  /// @brief Returns json array of all MaintenanceActions within the deque
  Result toJson(VPackBuilder& builder);

  /// @brief Return pointer to next ready action, or nullptr
  std::shared_ptr<maintenance::Action> findReadyAction(
      std::unordered_set<std::string> const& options = std::unordered_set<std::string>());

  /// @brief Process specific ID for a new action
  /// @returns uint64_t
  uint64_t nextActionId() { return _nextActionId++; }

  bool isShuttingDown() const { return (_isShuttingDown); }

  /// @brief Return number of seconds to say "not done" to block retries too soon
  uint32_t getSecondsActionsBlock() const { return _secondsActionsBlock; }

  /**
   * @brief Find and return first found not-done action or nullptr
   * @param desc Description of sought action
   */
  std::shared_ptr<maintenance::Action> findFirstNotDoneAction(
      std::shared_ptr<maintenance::ActionDescription> const& desc);

  /**
   * @brief add index error to bucket
   *        Errors are added by EnsureIndex
   *
   * @param  database     database
   * @param  collection   collection
   * @param  shard        shard
   * @param  indexId      index' id
   *
   * @return success
   */
  arangodb::Result storeIndexError(std::string const& database, std::string const& collection,
                                   std::string const& shard, std::string const& indexId,
                                   std::shared_ptr<VPackBuffer<uint8_t>> error);

  /**
   * @brief remove 1+ errors from index error bucket
   *        Errors are removed by phaseOne, as soon as indexes no longer in plan
   *
   * @param  database     database
   * @param  collection   collection
   * @param  shard        shard
   * @param  indexId      index' id
   *
   * @return success
   */
  arangodb::Result removeIndexErrors(std::string const& database,
                                     std::string const& collection, std::string const& shard,
                                     std::unordered_set<std::string> const& indexIds);
  arangodb::Result removeIndexErrors(std::string const& path,
                                     std::unordered_set<std::string> const& indexIds);

  /**
   * @brief add shard error to bucket
   *        Errors are added by CreateCollection, UpdateCollection
   *
   * @param  database     database
   * @param  collection   collection
   * @param  shard        shard
   *
   * @return success
   */
  arangodb::Result storeShardError(std::string const& database,
                                   std::string const& collection, std::string const& shard,
                                   std::shared_ptr<VPackBuffer<uint8_t>> error);

  arangodb::Result storeShardError(std::string const& database, std::string const& collection,
                                   std::string const& shard, std::string const& serverId,
                                   arangodb::Result const& failure);

  /**
   * @brief get all pending shard errors
   *
   * @param  database     database
   * @param  collection   collection
   * @param  shard        shard
   *
   * @return success
   */
  arangodb::Result shardError(std::string const& database,
                              std::string const& collection, std::string const& shard,
                              std::shared_ptr<VPackBuffer<uint8_t>>& error) const;

  /**
   * @brief remove error from shard bucket
   *        Errors are removed by phaseOne, as soon as indexes no longer in plan
   *
   * @param  database     database
   * @param  collection   collection
   * @param  shard        shard
   *
   * @return success
   */
  arangodb::Result removeShardError(std::string const& database, std::string const& collection,
                                    std::string const& shard);
  arangodb::Result removeShardError(std::string const& key);

  /**
   * @brief add shard error to bucket
   *        Errors are added by CreateCollection, UpdateCollection
   *
   * @param  database     database
   *
   * @return success
   */
  arangodb::Result storeDBError(std::string const& database,
                                std::shared_ptr<VPackBuffer<uint8_t>> error);

  arangodb::Result storeDBError(std::string const& database, Result const& failure);

  /**
   * @brief get all pending shard errors
   *
   * @param  database     database
   *
   * @return success
   */
  arangodb::Result dbError(std::string const& database,
                           std::shared_ptr<VPackBuffer<uint8_t>>& error) const;

  /**
   * @brief remove an error from db error bucket
   *        Errors are removed by phaseOne, as soon as indexes no longer in plan
   *
   * @param  database     database
   *
   * @return success
   */
  arangodb::Result removeDBError(std::string const& database);

  /**
   * @brief copy all error maps (shards, indexes and databases) for Maintenance
   *
   * @param  errors  errors struct into which all maintenance feature error are copied
   * @return         success
   */
  arangodb::Result copyAllErrors(errors_t& errors) const;

  /**
   * @brief get volatile shard version
   */
  uint64_t shardVersion(std::string const& shardId) const;

  /**
   * @brief increment volatile local shard version
   */
  uint64_t incShardVersion(std::string const& shardId);

  /**
   * @brief clean up after shard has been dropped locally
   * @param  shard  Shard name
   */
  void delShardVersion(std::string const& shardId);

  /**
   * @brief mark and list dirty databases
   */
  void addDirty(std::string const& database);
  void addDirty(std::unordered_set<std::string> const& databases, bool callNotify);
  std::unordered_set<std::string> dirty(
    std::unordered_set<std::string> const& = std::unordered_set<std::string>());
  /// @brief get n random db names
  std::unordered_set<std::string> pickRandomDirty (size_t n);


 protected:
  void initializeMetrics();

 private:
  /// @brief Search for first action matching hash and predicate
  /// @return shared pointer to action object if exists, empty shared_ptr if not
  std::shared_ptr<maintenance::Action> findFirstActionHash(
      size_t hash,
      std::function<bool(std::shared_ptr<maintenance::Action> const&)> const& predicate);

  /// @brief Search for first action matching hash and predicate (with lock already held by caller)
  /// @return shared pointer to action object if exists, empty shared_ptr if not
  std::shared_ptr<maintenance::Action> findFirstActionHashNoLock(
      size_t hash,
      std::function<bool(std::shared_ptr<maintenance::Action> const&)> const& predicate);

  /// @brief Search for action by Id
  /// @return shared pointer to action object if exists, nullptr if not
  std::shared_ptr<maintenance::Action> findActionId(uint64_t id);

  /// @brief Search for action by Id (but lock already held by caller)
  /// @return shared pointer to action object if exists, nullptr if not
  std::shared_ptr<maintenance::Action> findActionIdNoLock(uint64_t hash);

  /// @brief collect all database names
  std::unordered_set<std::string> allDatabases() const;

  /// @brief refill local database list for future random checking
  void refillToCheck();

 protected:
  /// @brief option for forcing this feature to always be enable - used by the catch tests
  bool _forceActivation;

  bool _resignLeadershipOnShutdown;

  /// @brief detect fresh start
  bool _firstRun;

  /// @brief tunable option for thread pool size
  uint32_t _maintenanceThreadsMax;

  /// @brief tunable option for number of seconds COMPLETE or FAILED actions block
  ///  duplicates from adding to _actionRegistry
  int32_t _secondsActionsBlock;

  /// @brief tunable option for number of seconds COMPLETE and FAILE actions remain
  ///  within _actionRegistry
  int32_t _secondsActionsLinger;

  /// @brief flag to indicate when it is time to stop thread pool
  std::atomic<bool> _isShuttingDown;

  /// @brief simple counter for creating MaintenanceAction id.  Ok for it to roll over.
  std::atomic<uint64_t> _nextActionId;

  //
  // Lock notes:
  //  Reading _actionRegistry requires Read or Write lock via
  //  _actionRegistryLock Writing _actionRegistry requires BOTH:
  //    - CONDITION_LOCKER on _actionRegistryCond
  //    - then write lock via _actionRegistryLock
  //
  /// @brief all actions executing, waiting, and done
  std::deque<std::shared_ptr<maintenance::Action>> _actionRegistry;

  // The following is protected with the _actionRegistryLock exactly as
  // the _actionRegistry. This priority queue is used to find the highest
  // priority action that is ready. Therefore, _prioQueue contains all the
  // actions in state READY. The sorting is done such that all fast track
  // actions come before all non-fast track actions. Therefore, a fast track
  // thread can just look at the top action and if this is not fast track,
  // it does not have to pop anything. If a worker picks an action and starts
  // work on it, the action leaves state READY and is popped from the priority
  // queue.
  // We also need to be able to delete actions which are READY. In that case
  // we need to leave the action in _prioQueue (since we cannot remove anything
  // but the top from it), and simply put it into a different state.
  std::priority_queue<std::shared_ptr<maintenance::Action>,
                      std::vector<std::shared_ptr<maintenance::Action>>, SharedPtrComparer<maintenance::Action>>
      _prioQueue;

  /// @brief lock to protect _actionRegistry and state changes to MaintenanceActions within
  mutable arangodb::basics::ReadWriteLock _actionRegistryLock;

  /// @brief condition variable to motivate workers to find new action
  arangodb::basics::ConditionVariable _actionRegistryCond;

  /// @brief list of background workers
  std::vector<std::unique_ptr<maintenance::MaintenanceWorker>> _activeWorkers;

  /// @brief condition variable to indicate thread completion
  arangodb::basics::ConditionVariable _workerCompletion;

  /// Errors are managed through raiseIndexError / removeIndexError and
  /// raiseShardError / renoveShardError. According locks must be held in said
  /// methods.

  /// @brief lock for index error bucket
  mutable arangodb::Mutex _ieLock;
  /// @brief pending errors raised by EnsureIndex
  std::map<std::string, std::map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>>> _indexErrors;

  /// @brief lock for shard error bucket
  mutable arangodb::Mutex _seLock;
  /// @brief pending errors raised by CreateCollection/UpdateCollection
  std::unordered_map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>> _shardErrors;

  /// @brief lock for database error bucket
  mutable arangodb::Mutex _dbeLock;
  /// @brief pending errors raised by CreateDatabase
  std::unordered_map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>> _dbErrors;

  /// @brief lock for shard version map
  mutable arangodb::Mutex _versionLock;
  /// @brief shards have versions in order to be able to distinguish between
  /// independant actions
  std::unordered_map<std::string, size_t> _shardVersion;

  std::atomic<std::chrono::steady_clock::duration> _pauseUntil;

  /// @brief shard action map, this map holds information which job (can only
  /// be one) is currently scheduled or executing for a given shard name. An
  /// entry is added whenever an ActionDescription is created in Maintenance
  /// and is removed, when the action for the shard is finished. The main Maintenance
  /// loop with phaseOne and phaseTwo creates a copy of this map before it does
  /// getLocalCollections and then avoids pondering over any shard which has an
  /// entry in the map. In this way, shard deliberations as well as shard actions
  /// are serialized and only one is happening at a time.
  ShardActionMap _shardActionMap;

  /// @brief mutex protecting _shardActionMap
  mutable std::mutex _shardActionMapMutex;

  std::vector<std::string> _databasesToCheck;
  size_t _lastNumberOfDatabases;

 public:
  std::optional<std::reference_wrapper<Histogram<log_scale_t<uint64_t>>>> _phase1_runtime_msec;
  std::optional<std::reference_wrapper<Histogram<log_scale_t<uint64_t>>>> _phase2_runtime_msec;
  std::optional<std::reference_wrapper<Histogram<log_scale_t<uint64_t>>>> _agency_sync_total_runtime_msec;

  std::optional<std::reference_wrapper<Counter>> _phase1_accum_runtime_msec;
  std::optional<std::reference_wrapper<Counter>> _phase2_accum_runtime_msec;
  std::optional<std::reference_wrapper<Counter>> _agency_sync_total_accum_runtime_msec;

  std::optional<std::reference_wrapper<Counter>> _action_duplicated_counter;
  std::optional<std::reference_wrapper<Counter>> _action_registered_counter;
  std::optional<std::reference_wrapper<Counter>> _action_done_counter;

  struct ActionMetrics {
    Histogram<log_scale_t<uint64_t>>& _runtime_histogram;
    Histogram<log_scale_t<uint64_t>>& _queue_time_histogram;
    Counter& _accum_runtime;
    Counter& _accum_queue_time;
    Counter& _failure_counter;

    ActionMetrics(Histogram<log_scale_t<uint64_t>>& a,
                  Histogram<log_scale_t<uint64_t>>& b, Counter& c, Counter& d, Counter& e)
        : _runtime_histogram(a),
          _queue_time_histogram(b),
          _accum_runtime(c),
          _accum_queue_time(d),
          _failure_counter(e) {}
  };

  std::unordered_map<std::string, ActionMetrics> _maintenance_job_metrics_map;
  std::optional<std::reference_wrapper<Histogram<log_scale_t<uint64_t>>>> _maintenance_action_runtime_msec;

  std::optional<std::reference_wrapper<Gauge<uint64_t>>> _shards_out_of_sync;
  std::optional<std::reference_wrapper<Gauge<uint64_t>>> _shards_total_count;
  std::optional<std::reference_wrapper<Gauge<uint64_t>>> _shards_leader_count;
  std::optional<std::reference_wrapper<Gauge<uint64_t>>> _shards_not_replicated_count;
};

}  // namespace arangodb

#endif
