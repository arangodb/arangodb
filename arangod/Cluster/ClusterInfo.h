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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Agency/AgencyComm.h"
#include "Agency/AgencyCommon.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/CallbackGuard.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/RebootTracker.h"
#include "Metrics/Fwd.h"
#include "Metrics/MetricsResourceMonitor.h"
#include "Network/types.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/Version.h"

#include "Basics/ResourceUsage.h"

struct TRI_vocbase_t;

namespace arangodb {

namespace futures {
template<typename T>
class Future;
template<typename T>
class Promise;
}  // namespace futures

class Result;
template<typename T>
class ResultT;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace replication2 {
class LogId;
namespace agency {
struct LogPlanSpecification;
struct LogTarget;
}  // namespace agency
namespace replicated_state::agency {
struct Target;
}  // namespace replicated_state::agency
}  // namespace replication2

class AgencyCallbackRegistry;
class AgencyCache;
struct ClusterCollectionCreationInfo;
class ClusterInfo;
class CollectionInfoCurrent;
class CreateDatabaseInfo;
class IndexId;
class LogicalDataSource;
class LogicalCollection;
class LogicalView;

class AnalyzerModificationTransaction {
 public:
  using Ptr = std::unique_ptr<AnalyzerModificationTransaction>;

  AnalyzerModificationTransaction(std::string_view database, ClusterInfo* ci,
                                  bool cleanup);

  AnalyzerModificationTransaction(AnalyzerModificationTransaction const&) =
      delete;
  AnalyzerModificationTransaction& operator=(
      AnalyzerModificationTransaction const&) = delete;

  ~AnalyzerModificationTransaction();

  static int32_t getPendingCount() noexcept;

  [[nodiscard]] AnalyzersRevision::Revision buildingRevision() const noexcept;

  Result start();
  Result commit();
  Result abort();

 private:
  void revertCounter();

  // our cluster info
  ClusterInfo* _clusterInfo;

  // database for operation
  // need to copy as may be temp value provided to constructor
  DatabaseID _database;

  // rollback operations counter
  bool _rollbackCounter{false};

  // rollback revision in Plan
  bool _rollbackRevision{false};

  // cleanup or normal analyzer insert/remove
  bool _cleanupTransaction;

  // revision this transaction is building
  // e.g. revision will be current upon successful commit of
  // this transaction. For cleanup transaction this equals to current revision
  // as cleanup just cleans the mess and reverts revision back to normal
  AnalyzersRevision::Revision _buildingRevision{AnalyzersRevision::LATEST};

  // pending operation sount. Positive number means some operations are ongoing
  // zero means system idle
  // negative value means recovery is ongoing
  static inline std::atomic_int32_t _pendingAnalyzerOperationsCount{0};
};

#ifdef ARANGODB_USE_GOOGLE_TESTS
class ClusterInfo {
#else
class ClusterInfo final {
#endif

 private:
  struct CollectionWithHash {
    uint64_t hash;
    std::shared_ptr<LogicalCollection> collection;
  };

  // IMPORTANT NOTE: This Hasher method does break ShardID.
  // If you create a Map based on ShardID, and using this Hasher
  // and then ask it for a string or string_view. This hasher will
  // use the string_view hash method for the search term, but used
  // the ShardID hash method for the inserted data, which are DIFFERENT.
  // The compiler and sanitizers are absolutely happy with this.
  // So it is on the developers duty to not make this mistake.
  struct Hasher {
    using is_transparent = void;
    template<typename T>
    size_t operator()(
        std::basic_string<char, std::char_traits<char>, T> const& str) const {
      return absl::Hash<std::string_view>{}(str);
    }
    template<typename T>
    size_t operator()(T const& v) const {
      return absl::Hash<T>{}(v);
    }
  };
  struct KeyEqual {
    using is_transparent = void;
    // special stunt for strings with different allocators.
    template<typename T, typename V>
    bool operator()(
        std::basic_string<char, std::char_traits<char>, T> const& lhs,
        std::basic_string<char, std::char_traits<char>, V> const& rhs) const {
      return std::string_view{lhs} == rhs;
    }
    template<typename T, typename V>
    bool operator()(T const& lhs, V const& rhs) const {
      return lhs == rhs;
    }
  };

  using ClusterInfoResourceMonitor =
      metrics::GaugeResourceMonitor<metrics::Gauge<std::uint64_t>>;

  template<typename T>
  using ClusterInfoResourceAllocator =
      ResourceUsageAllocator<T, ClusterInfoResourceMonitor>;

  struct pmr {
    using ManagedString = std::basic_string<char, std::char_traits<char>,
                                            ClusterInfoResourceAllocator<char>>;
    using ServerID = ManagedString;      // ID of a server
    using DatabaseID = ManagedString;    // ID/name of a database
    using CollectionID = ManagedString;  // ID of a collection
    using ViewID = ManagedString;        // ID of a view
  };

  template<typename K, typename V>
  using FlatMap = containers::FlatHashMap<
      K, V,
      std::conditional_t<std::is_same_v<K, ShardID>, absl::Hash<K>, Hasher>,
      KeyEqual, ClusterInfoResourceAllocator<std::pair<K const, V>>>;
  template<typename K, typename V>
  using FlatMapShared = containers::FlatHashMap<
      K, std::shared_ptr<V>,
      std::conditional_t<std::is_same_v<K, ShardID>, absl::Hash<K>, Hasher>,
      KeyEqual,
      ClusterInfoResourceAllocator<std::pair<K const, std::shared_ptr<V>>>>;

  template<typename K, typename V>
  using AssocMultiMap =
      std::multimap<K, V, KeyEqual,
                    ClusterInfoResourceAllocator<std::pair<K const, V>>>;

  template<typename T>
  using ManagedVector = std::vector<T, ClusterInfoResourceAllocator<T>>;

  using DatabaseCollections = FlatMap<pmr::CollectionID, CollectionWithHash>;
  using AllCollections =
      FlatMapShared<pmr::DatabaseID, DatabaseCollections const>;

  using DatabaseCollectionsCurrent =
      FlatMapShared<pmr::CollectionID, CollectionInfoCurrent>;

  using DatabaseViews = FlatMapShared<pmr::ViewID, LogicalView>;
  // TODO(MBkkt) Now on every loadPlan all DatabaseViews
  //  copied for every database, we can avoid it.
  using AllViews = FlatMap<pmr::DatabaseID, DatabaseViews>;

  class SyncerThread;

  template<typename T, typename... Args>
  auto allocateShared(Args&&... args) {
    return std::allocate_shared<T>(
        ClusterInfoResourceAllocator<T>{_resourceMonitor},
        std::forward<Args>(args)...);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initializes library
  /// We are a singleton class, therefore nobody is allowed to create
  /// new instances or copy them, except we ourselves.
  //////////////////////////////////////////////////////////////////////////////

 public:
  ClusterInfo(ClusterInfo const&) = delete;             // not implemented
  ClusterInfo& operator=(ClusterInfo const&) = delete;  // not implemented

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates library
  //////////////////////////////////////////////////////////////////////////////

  explicit ClusterInfo(ArangodServer& server, AgencyCache& agencyCache,
                       AgencyCallbackRegistry& agencyCallbackRegistry,
                       ErrorCode syncerShutdownCode,
                       metrics::MetricsFeature& metrics);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief shuts down library
  //////////////////////////////////////////////////////////////////////////////

  TEST_VIRTUAL ~ClusterInfo();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cleanup method which frees cluster-internal shared ptrs on shutdown
  //////////////////////////////////////////////////////////////////////////////

  void unprepare();

  /**
   * @brief begins shutdown of cluster info,
   * begin shutting down plan and current syncers
   */
  void beginShutdown();

  /// @brief cancel all pending wait-for-syncer operations
  void drainSyncers();

  /**
   * @brief begin shutting down plan and current syncers
   */
  void startSyncers();

  /**
   * @brief wait for syncers' full stop
   */
  void waitForSyncersToStop();

  /// @brief produces an agency dump and logs it
  void logAgencyDump() const;

  /// @brief get database cache
  VPackBuilder toVelocyPack();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a number of cluster-wide unique IDs, returns the first
  /// one and guarantees that <number> are reserved for the caller.
  //////////////////////////////////////////////////////////////////////////////

  uint64_t uniqid(uint64_t = 1);

  /**
   * @brief Agency dump including replicated log and compaction
   * @param  body  Builder to fill with dump
   * @return       Operation's result
   */
  Result agencyDump(std::shared_ptr<VPackBuilder> const& body);

  /**
   * @brief Agency plan
   * @param  body  Builder to fill with copy of plan
   * @return       Operation's result
   */
  Result agencyPlan(std::shared_ptr<VPackBuilder> const& body);

  /**
   * @brief Overwrite agency plan
   * @param  plan  Plan to adapt to
   * @return       Operation's result
   */
  Result agencyReplan(VPackSlice plan);

  /**
   * @brief Wait for Plan cache to be at the given Raft index
   * @param raftIndex Raft index to wait for
   * @return Operation's result
   */
  [[nodiscard]] futures::Future<Result> waitForPlan(
      consensus::index_t raftIndex);

  /**
   * @brief Wait for Plan cache to be at the given Plan version
   * @param planVersion version to wait for
   * @return Operation's result
   */
  [[nodiscard]] futures::Future<Result> waitForPlanVersion(
      uint64_t planVersion);

  /**
   * @brief Fetch the current Plan version and wait for the cache to catch up to
   *        it, if necessary.
   * @param timeout is for fetching the Plan version only. Waiting for the
   *        Plan version afterwards will never timeout.
   */
  [[nodiscard]] futures::Future<Result> fetchAndWaitForPlanVersion(
      network::Timeout timeout) const;

  [[nodiscard]] futures::Future<Result> fetchAndWaitForCurrentVersion(
      network::Timeout timeout) const;

  /**
   * @brief Wait for Current cache to be at the given Raft index
   * @param raftIndex Raft index to wait for
   * @return Operation's result
   */
  futures::Future<Result> waitForCurrent(consensus::index_t raftIndex);

  /**
   * @brief Wait for Current cache to be at the given Raft index
   * @param currentVersion version to wait for
   * @return Operation's result
   */
  [[nodiscard]] futures::Future<Result> waitForCurrentVersion(
      uint64_t currentVersion);

  /**
   * Blocks and waits until all planned shards
   * have reported back in current and a leader
   * has thereby taken over the shard responsibility.
   * After this the shards are usable.
   *
   * Note: This method is written for hot-restore
   * As we erase the current during the process
   * and need to wait for current to be build back up.
   */
  void syncWaitForAllShardsToEstablishALeader();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief flush the caches (used for testing only)
  //////////////////////////////////////////////////////////////////////////////

  void flush();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask whether a cluster database exists
  //////////////////////////////////////////////////////////////////////////////

  bool doesDatabaseExist(std::string_view databaseID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get list of databases in the cluster
  //////////////////////////////////////////////////////////////////////////////

  std::vector<DatabaseID> databases();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection
  /// Throwing version, deprecated.
  //////////////////////////////////////////////////////////////////////////////

  TEST_VIRTUAL std::shared_ptr<LogicalCollection> getCollection(
      std::string_view databaseID, std::string_view collectionID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection
  /// If it is not found in the cache, the cache is reloaded once. The second
  /// argument can be a collection ID or a collection name (both cluster-wide).
  /// will not throw but return nullptr if the collection isn't found.
  //////////////////////////////////////////////////////////////////////////////

  TEST_VIRTUAL std::shared_ptr<LogicalCollection> getCollectionNT(
      std::string_view databaseID, std::string_view collectionID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection or a view
  /// If it is not found in the cache, the cache is reloaded once. The second
  /// argument can be a collection ID or a collection name (both cluster-wide)
  /// or a view ID or name.
  /// will not throw but return nullptr if the collection/view isn't found.
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<LogicalDataSource> getCollectionOrViewNT(
      std::string_view databaseID, std::string_view name);

  //////////////////////////////////////////////////////////////////////////////
  /// Format error message for TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
  //////////////////////////////////////////////////////////////////////////////
  static std::string getCollectionNotFoundMsg(std::string_view databaseID,
                                              std::string_view collectionID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about all collections of a database
  //////////////////////////////////////////////////////////////////////////////

  TEST_VIRTUAL std::vector<std::shared_ptr<LogicalCollection>> getCollections(
      std::string_view databaseID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Generate Collection Stubs during Database buiding
  /// These stubs are no real collection, use this API with care
  /// and only if the database is in building state.
  //////////////////////////////////////////////////////////////////////////////

  [[nodiscard]] std::unordered_map<std::string,
                                   std::shared_ptr<LogicalCollection>>
  generateCollectionStubs(TRI_vocbase_t& database);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a view
  /// If it is not found in the cache, the cache is reloaded once. The second
  /// argument can be a collection ID or a view name (both cluster-wide).
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<LogicalView> getView(std::string_view databaseID,
                                       std::string_view viewID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about all views of a database
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::shared_ptr<LogicalView>> getViews(
      std::string_view databaseID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about analyzers revision
  /// @param databaseID database name
  /// @param forceLoadPlan always reload plan
  //////////////////////////////////////////////////////////////////////////////
  AnalyzersRevision::Ptr getAnalyzersRevision(std::string_view databaseID,
                                              bool forceLoadPlan = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reads analyzers revisions needed for querying specified database.
  ///        Could trigger plan load if database is not found in plan.
  /// @param databaseID database to query
  /// @return extracted revisions
  //////////////////////////////////////////////////////////////////////////////
  QueryAnalyzerRevisions getQueryAnalyzersRevision(std::string_view databaseID);

  /// @brief return shard statistics for the specified database,
  /// optionally restricted to anything on the specified server
  Result getShardStatisticsForDatabase(std::string const& databaseID,
                                       std::string_view restrictServer,
                                       velocypack::Builder& builder) const;

  /// @brief return shard statistics for all databases, totals,
  /// optionally restricted to anything on the specified server
  Result getShardStatisticsGlobal(std::string const& restrictServer,
                                  velocypack::Builder& builder) const;

  /// @brief return shard statistics, separate for each database,
  /// optionally restricted to anything on the specified server
  Result getShardStatisticsGlobalDetailed(std::string const& restrictServer,
                                          velocypack::Builder& builder) const;

  /// @brief get shard statistics for all databases, split by servers.
  Result getShardStatisticsGlobalByServer(VPackBuilder& builder) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection in current. This returns information about
  /// all shards in the collection.
  /// If it is not found in the cache, the cache is reloaded once.
  //////////////////////////////////////////////////////////////////////////////

  TEST_VIRTUAL std::shared_ptr<CollectionInfoCurrent> getCollectionCurrent(
      std::string_view databaseID, std::string_view collectionID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the RebootTracker
  //////////////////////////////////////////////////////////////////////////////

  cluster::RebootTracker& rebootTracker() noexcept;
  cluster::RebootTracker const& rebootTracker() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Test if all names (Collection & Views) are available  in the given
  /// database and return the planVersion this can be guaranteed on.
  //////////////////////////////////////////////////////////////////////////////

  ResultT<uint64_t> checkDataSourceNamesAvailable(
      std::string_view databaseName, std::vector<std::string> const& names);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create database in coordinator
  ///
  /// A database is first created in the isBuilding state, and therefore not
  ///  visible or usable to the outside world.
  ///
  /// After the database has been fully setup, it is then confirmed created
  /// and becomes visible and usable.
  ///
  /// If any error happens on the way, a pending database will be cleaned up
  ///
  //////////////////////////////////////////////////////////////////////////////
  Result createIsBuildingDatabaseCoordinator(
      CreateDatabaseInfo const& database);
  Result createFinalizeDatabaseCoordinator(CreateDatabaseInfo const& database);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes database and collection entries within the agency plan
  //////////////////////////////////////////////////////////////////////////////
  Result cancelCreateDatabaseCoordinator(CreateDatabaseInfo const& database);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop database in coordinator
  //////////////////////////////////////////////////////////////////////////////
  Result dropDatabaseCoordinator(  // drop database
      std::string const& name,     // database name
      double timeout               // request timeout
  );

#ifdef ARANGODB_USE_GOOGLE_TESTS
  /***
   * This section of methods is only required for our C++ UnitTests
   * They have been using very low level APIs and cannot be trivially
   * moved to new higher level APIs.
   */

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a future which can be used to wait for the successful
  /// creation of replicated states
  //////////////////////////////////////////////////////////////////////////////
  auto waitForReplicatedStatesCreation(
      std::string const& databaseName,
      std::vector<replication2::agency::LogTarget> const& replicatedStates)
      -> futures::Future<Result>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief deletes replicated states corresponding to shards
  //////////////////////////////////////////////////////////////////////////////
  auto deleteReplicatedStates(
      std::string const& databaseName,
      std::vector<replication2::LogId> const& replicatedStatesIds)
      -> futures::Future<Result>;

  /// @brief this method does an atomic check of the preconditions for the
  /// collections to be created, using the currently loaded plan.
  Result checkCollectionPreconditions(
      std::string const& databaseName,
      std::vector<ClusterCollectionCreationInfo> const& infos);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create collection in coordinator
  //////////////////////////////////////////////////////////////////////////////
  Result createCollectionCoordinator(   // create collection
      std::string const& databaseName,  // database name
      std::string const& collectionID, uint64_t numberOfShards,
      uint64_t replicationFactor, uint64_t writeConcern,
      bool waitForReplication, velocypack::Slice json,
      double timeout,  // request timeout
      bool isNewDatabase,
      std::shared_ptr<LogicalCollection> const& colToDistributeShardsLike,
      replication::Version replicationVersion);

  /// @brief create multiple collections in coordinator
  ///        If any one of these collections fails, all creations will be
  ///        rolled back.
  /// Note that in contrast to most other methods here, this method does not
  /// get a timeout parameter, but an endTime parameter!!!
  Result createCollectionsCoordinator(
      std::string const& databaseName,
      std::vector<ClusterCollectionCreationInfo>&, double endTime,
      bool isNewDatabase,
      std::shared_ptr<LogicalCollection const> const& colToDistributeShardsLike,
      replication::Version replicationVersion);

#endif

  /// @brief drop collection in coordinator
  //////////////////////////////////////////////////////////////////////////////
  Result dropCollectionCoordinator(     // drop collection
      std::string const& databaseName,  // database name
      std::string const& collectionID,  // collection identifier
      double timeout                    // request timeout
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create view in coordinator
  //////////////////////////////////////////////////////////////////////////////
  Result createViewCoordinator(         // create view
      std::string const& databaseName,  // database name
      std::string const& viewID,        // view identifier
      velocypack::Slice json            // view definition
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop view in coordinator
  //////////////////////////////////////////////////////////////////////////////
  Result dropViewCoordinator(           // drop view
      std::string const& databaseName,  // database name
      std::string const& viewID         // view identifier
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set view properties in coordinator
  //////////////////////////////////////////////////////////////////////////////

  Result setViewPropertiesCoordinator(std::string const& databaseName,
                                      std::string const& viewID,
                                      VPackSlice json);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief start creating or deleting an analyzer in coordinator,
  /// the return value is an ArangoDB error code and building revision number if
  /// succeeded, AnalyzersRevision::LATEST on error and the errorMsg is set
  /// accordingly. One possible error is a timeout.
  /// @note should not be called directly - use AnalyzerModificationTransaction
  //////////////////////////////////////////////////////////////////////////////
  std::pair<Result, AnalyzersRevision::Revision>
  startModifyingAnalyzerCoordinator(std::string_view databaseID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finish creating or deleting an analyzer in coordinator,
  /// the return value is an ArangoDB error code
  /// and the errorMsg is set accordingly. One possible error
  /// is a timeout.
  /// @note should not be called directly - use AnalyzerModificationTransaction
  //////////////////////////////////////////////////////////////////////////////

  Result finishModifyingAnalyzerCoordinator(std::string_view databaseId,
                                            bool restore);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Init metrics state
  /// @note Current server should be Coordinator
  /// Try to propose current server as leader or know that someone
  /// was a leader. No return while we don't know that or server should stop.
  //////////////////////////////////////////////////////////////////////////////
  void initMetricsState();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief The MetricsState that stored in agency.
  /// @note If `leader` is `nullopt` that means we ourselves are the leader.
  //////////////////////////////////////////////////////////////////////////////
  struct [[nodiscard]] MetricsState {
    std::optional<ServerID> leader;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Gets the current MetricsState from the agencyCache.
  /// @param wantLeader If it is `true`, a RebootTracker event is set,
  /// in which we will try to become the new leader ourselves, if the current
  /// leader dies. If the flag is `false`, no new event is set,
  /// but the old one is also not deleted.
  /// @note This method can throw exceptions of various types, if
  /// something is not founded or some other error occurs, so the called
  /// must guard against this. In particular, this can happen in the
  /// bootstrap phase if the `AgencyCache` has not yet heard about this.
  /// @note If `wantLeader` is true, then thread-safe, otherwise not
  /// @return Who is the leader, and what cache version is current.
  //////////////////////////////////////////////////////////////////////////////
  MetricsState getMetricsState(bool wantLeader);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Try to propose current server as new leader
  /// @note Current server should be Coordinator
  /// @param oldRebootId last leader RebootId
  /// @param oldServerId last leader ServerID
  //////////////////////////////////////////////////////////////////////////////
  void proposeMetricsLeader(uint64_t oldRebootId, std::string_view oldServerId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Creates cleanup transaction for first found dangling operation
  /// @return created transaction or nullptr if no cleanup needed
  //////////////////////////////////////////////////////////////////////////////

  AnalyzerModificationTransaction::Ptr createAnalyzersCleanupTrans();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about servers from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadServers();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the endpoint of a server from its ID.
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there an empty string is returned as an error.
  //////////////////////////////////////////////////////////////////////////////

  std::string getServerEndpoint(std::string_view serverID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the advertised endpoint of a server from its ID.
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there an empty string is returned as an error.
  //////////////////////////////////////////////////////////////////////////////

  std::string getServerAdvertisedEndpoint(std::string_view serverID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the server ID for an endpoint.
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there an empty string is returned as an error.
  //////////////////////////////////////////////////////////////////////////////

  std::string getServerName(std::string_view endpoint);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about all coordinators from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadCurrentCoordinators();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the mappings between different IDs/names from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadCurrentMappings();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about all DBservers from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadCurrentDBServers();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a list of all DBServers in the cluster that have
  /// currently registered
  //////////////////////////////////////////////////////////////////////////////

  std::vector<ServerID> getCurrentDBServers();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the servers who are responsible for a shard (one leader
  /// and possibly multiple followers).
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there a pointer to an empty vector is returned as
  /// an error.
  /// The "NoDelay" variant is guaranteed to not produce a noticeable delay,
  /// however, it may return an empty result, if currently there is no
  /// known responsible server. This is often good enough.
  /// Note that this can happen during the transition of leadership from
  /// one server to another.
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<ManagedVector<pmr::ServerID> const> getResponsibleServer(
      ShardID shardID);
  std::shared_ptr<ManagedVector<pmr::ServerID> const>
  getResponsibleServerNoDelay(ShardID shardID);

  futures::Future<ResultT<ServerID>> getLeaderForShard(ShardID shard);

  futures::Future<Result> getLeadersForShards(std::span<ShardID const> shard,
                                              std::span<ServerID> result);

  enum class ShardLeadership { kLeader, kFollower, kUnclear };
  ShardLeadership getShardLeadership(ServerID const& server,
                                     ShardID const& shard) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief atomically find all servers who are responsible for the given
  /// shards (only the leaders).
  /// will throw an exception if no leader can be found for any
  /// of the shards. will return an empty result if the shards couldn't be
  /// determined after a while - it is the responsibility of the caller to
  /// check for an empty result!
  //////////////////////////////////////////////////////////////////////////////

  containers::FlatHashMap<ShardID, ServerID> getResponsibleServers(
      containers::FlatHashSet<ShardID> const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief atomically find all servers who are responsible for the given
  /// shards (choose either the leader or some follower for each, but
  /// make the choice consistent with `distributeShardsLike` dependencies.
  /// Will throw an exception if no leader can be found for any
  /// of the shards. Will return an empty result if the shards couldn't be
  /// determined after a while - it is the responsibility of the caller to
  /// check for an empty result!
  /// The map `result` can already contain a partial choice, this method
  /// ensures that all the shards in `list` are in the end set in the
  /// `result` map. Additional shards can be added to `result` as needed,
  /// in particular the shard prototypes of the shards in list will be added.
  /// It is not allowed that `result` contains a setting for a shard but
  /// no setting (or a different one) for its shard prototype!
  //////////////////////////////////////////////////////////////////////////////

#ifdef USE_ENTERPRISE
  void getResponsibleServersReadFromFollower(
      containers::FlatHashSet<ShardID> const& list,
      containers::FlatHashMap<ShardID, ServerID>& result);
#endif

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the shard list of a collection, sorted numerically
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<std::vector<ShardID> const> getShardList(
      std::string_view collectionID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the current list of (in-sync, for replication 1) servers of a
  /// shard
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<ManagedVector<pmr::ServerID> const> getCurrentServersForShard(
      ShardID shardId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the list of coordinator server names
  //////////////////////////////////////////////////////////////////////////////

  std::vector<ServerID> getCurrentCoordinators();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lookup a full coordinator ID by short ID
  //////////////////////////////////////////////////////////////////////////////

  ServerID getCoordinatorByShortID(ServerShortID shortId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidate current coordinators
  //////////////////////////////////////////////////////////////////////////////

  void invalidateCurrentCoordinators();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get current "Plan" structure
  //////////////////////////////////////////////////////////////////////////////
  containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder const>>
  getPlan(uint64_t& planIndex, containers::FlatHashSet<std::string> const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get current "Current" structure
  //////////////////////////////////////////////////////////////////////////////
  containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder const>>
  getCurrent(uint64_t& currentIndex,
             containers::FlatHashSet<std::string> const&);

  containers::FlatHashSet<ServerID> getFailedServers() const;

  void setFailedServers(containers::FlatHashSet<ServerID> failedServers);

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setServers(containers::FlatHashMap<ServerID, std::string> servers);

  void setServerAliases(containers::FlatHashMap<ServerID, std::string> aliases);

  void setServerAdvertisedEndpoints(
      containers::FlatHashMap<ServerID, std::string> advertisedEndpoints);

  void setShardToShardGroupLeader(
      containers::FlatHashMap<ShardID, ShardID> shardToShardGroupLeader);

  void setShardGroups(
      containers::FlatHashMap<ShardID, std::shared_ptr<std::vector<ShardID>>>);

  void setShardIds(containers::FlatHashMap<
                   ShardID, std::shared_ptr<std::vector<ServerID> const>>
                       shardIds);
#endif

  bool serverExists(std::string_view serverID) const noexcept;

  bool serverAliasExists(std::string_view alias) const noexcept;

  containers::FlatHashMap<ServerID, std::string> getServers();

  TEST_VIRTUAL containers::FlatHashMap<ServerID, std::string>
  getServerAliases();

  ServersKnown rebootIds() const;

  uint64_t getPlanVersion() const {
    READ_LOCKER(guard, _planProt.lock);
    return _planVersion;
  }

  uint64_t getPlanIndex() const {
    READ_LOCKER(readLocker, _planProt.lock);
    return _planIndex;
  }

  uint64_t getCurrentVersion() const {
    READ_LOCKER(guard, _currentProt.lock);
    return _currentVersion;
  }

  /**
   * @brief Get sorted list of DB server, which serve a shard
   *
   * @param shardId  The id of said shard
   * @return         List of DB servers serving the shard
   */
  Result getShardServers(ShardID shardId, std::vector<ServerID>&);

  /// @brief map shardId to collection name (not ID)
  CollectionID getCollectionNameForShard(ShardID shardId);
  CollectionID getCollectionNameForShard(
      basics::ReadLocker<basics::ReadWriteLock>&, ShardID shardId);
  /// @brief map shardId to database name (not ID)
  auto getDatabaseNameForShard(ShardID shardId) -> std::optional<DatabaseID>;
  auto getDatabaseNameForShard(basics::ReadLocker<basics::ReadWriteLock>&,
                               ShardID shardId) -> std::optional<DatabaseID>;

  auto getReplicatedLogLeader(replication2::LogId) const -> ResultT<ServerID>;

  auto getReplicatedLogParticipants(replication2::LogId) const
      -> ResultT<std::vector<ServerID>>;

  auto getReplicatedLogPlanSpecification(replication2::LogId) const -> ResultT<
      std::shared_ptr<replication2::agency::LogPlanSpecification const>>;

  auto getReplicatedLogsParticipants(std::string_view database) const
      -> ResultT<
          std::unordered_map<replication2::LogId, std::vector<std::string>>>;

  auto getCollectionGroupById(replication2::agency::CollectionGroupId)
      -> std::shared_ptr<
          replication2::agency::CollectionGroupPlanSpecification const>;

  /**
   * @brief Lock agency's hot backup with TTL 60 seconds
   *
   * @param  timeout  Timeout to wait for in seconds
   * @return          Operation's result
   */
  Result agencyHotBackupLock(std::string_view uuid, double timeout,
                             bool& supervisionOff);

  /**
   * @brief Lock agency's hot backup with TTL 60 seconds
   *
   * @param  timeout  Timeout to wait for in seconds
   * @return          Operation's result
   */
  Result agencyHotBackupUnlock(std::string_view uuid, double timeout,
                               bool supervisionOff);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get an operation timeout
  //////////////////////////////////////////////////////////////////////////////

  static constexpr double getTimeout(double timeout) {
    if (timeout == 0.0) {
      return 24.0 * 3600.0;
    }
    return timeout;
  }

  ArangodServer& server() const;

  AgencyCallbackRegistry& agencyCallbackRegistry() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the poll interval
  //////////////////////////////////////////////////////////////////////////////

  static constexpr double getPollInterval() { return 5.0; }

 private:
  /// @brief create a new collection object from the data, using the cache if
  /// possible
  CollectionWithHash buildCollection(
      bool isBuilding, AllCollections::const_iterator existingCollections,
      std::string_view collectionId, velocypack::Slice data,
      TRI_vocbase_t& vocbase, uint64_t planVersion, bool cleanupLinks) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about our plan
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  auto loadPlan() -> consensus::index_t;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about current state
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  auto loadCurrent() -> consensus::index_t;

  static void buildIsBuildingSlice(CreateDatabaseInfo const& database,
                                   VPackBuilder& builder);

  static void buildFinalSlice(CreateDatabaseInfo const& database,
                              VPackBuilder& builder);

  Result waitForDatabaseInCurrent(CreateDatabaseInfo const& database,
                                  AgencyWriteTransaction const& trx);

  void triggerWaiting(AssocMultiMap<uint64_t, futures::Promise<Result>>& mm,
                      uint64_t commitIndex);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the timeout for reloading the server list
  //////////////////////////////////////////////////////////////////////////////

  static double getReloadServerListTimeout() { return 60.0; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief triggers a new background thread to obtain the next batch of ids
  //////////////////////////////////////////////////////////////////////////////
  void triggerBackgroundGetIds();

  /// underlying application server
  ArangodServer& _server;
  ClusterFeature& _clusterFeature;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief object for agency communication
  //////////////////////////////////////////////////////////////////////////////

  AgencyComm _agency;

  AgencyCache& _agencyCache;
  AgencyCallbackRegistry& _agencyCallbackRegistry;

  // Cached data from the agency, we reload whenever necessary:

  // We group the data, each group has an atomic "valid-flag" which is
  // used for lazy loading in the beginning. It starts as false, is set
  // to true at each reload and is only reset to false if the cache
  // needs to be invalidated. The variable is atomic to be able to check
  // it without acquiring the read lock (see below). Flush is just an
  // explicit reload for all data and is only used in tests.
  // Furthermore, each group has a mutex that protects against
  // simultaneously contacting the agency for an update.
  // In addition, each group has two atomic version numbers, these are
  // used to prevent a stampede if multiple threads notice concurrently
  // that an update from the agency is necessary. Finally, there is a
  // read/write lock which protects the actual data structure.
  // We encapsulate this protection in the struct ProtectionData:

  struct ProtectionData {
    std::atomic<bool> isValid;
    mutable std::mutex mutex;
    std::atomic<uint64_t> wantedVersion;
    std::atomic<uint64_t> doneVersion;
    mutable basics::ReadWriteLock lock;

    ProtectionData() : isValid(false), wantedVersion(0), doneVersion(0) {}
  };

  cluster::RebootTracker _rebootTracker;
  cluster::CallbackGuard _metricsGuard;
  /// @brief error code sent to all remaining promises of the syncers at
  /// shutdown. normally this is TRI_ERROR_SHUTTING_DOWN, but it can be
  /// overridden during testing
  ErrorCode const _syncerShutdownCode;

  metrics::Gauge<std::uint64_t>& _memoryUsage;
  /// @brief histogram for loadPlan runtime
  metrics::Histogram<metrics::LogScale<float>>& _lpTimer;
  /// @brief histogram for loadCurrent runtime
  metrics::Histogram<metrics::LogScale<float>>& _lcTimer;

  ClusterInfoResourceMonitor _resourceMonitor;

  // The servers, first all, we only need Current here:
  FlatMap<pmr::ServerID, pmr::ManagedString>
      _servers;  // from Current/ServersRegistered
  FlatMap<pmr::ServerID, pmr::ManagedString>
      _serverAliases;  // from Current/ServersRegistered
  FlatMap<pmr::ServerID, pmr::ManagedString>
      _serverAdvertisedEndpoints;  // from Current/ServersRegistered
  FlatMap<pmr::ServerID, pmr::ManagedString>
      _serverTimestamps;  // from Current/ServersRegistered
  ProtectionData _serversProt;

  // TODO: Looks like this is used only in rebootIds() call
  // and set only together with rebootTracker (the same data).
  // So should we consider removing this member and use only rebootTracker?
  // Current/ServersKnown:
  ServersKnown _serversKnown;

  // Accounting drops of dangling links. We do not want to pollute
  // scheduler with drop requests. So we put only one per link at time.
  // And only if that request fails, we will try again.
  containers::FlatHashSet<std::uint64_t> _pendingCleanups;
  mutable containers::FlatHashSet<std::uint64_t> _currentCleanups;

  // The DBServers, also from Current:
  // from Current/DBServers
  FlatMap<pmr::ServerID, pmr::ServerID> _dbServers;
  ProtectionData _dbServersProt;

  // The Coordinators, also from Current:
  FlatMap<pmr::ServerID, pmr::ServerID>
      _coordinators;  // from Current/Coordinators
  ProtectionData _coordinatorsProt;

  // Mappings between short names/IDs and full server IDs
  FlatMap<ServerShortID, pmr::ServerID> _coordinatorIdMap;
  ProtectionData _mappingsProt;

  FlatMapShared<pmr::DatabaseID, VPackBuilder const> _plan;
  FlatMapShared<pmr::DatabaseID, VPackBuilder const> _current;
  FlatMap<pmr::DatabaseID, VPackSlice>
      _plannedDatabases;  // from Plan/Databases

  ProtectionData _planProt;

  uint64_t _planVersion;  // This is the version in the Plan which underlies
                          // the data in _plannedCollections and _shards
  uint64_t _planIndex;    // This is the Raft index, which corresponds to the
                          // above plan version
  uint64_t _planMemoryUsage;  // memory usage of VPack objects inside _plan

  FlatMapShared<pmr::DatabaseID, FlatMap<pmr::ServerID, VPackSlice> const>
      _currentDatabases;  // from Current/Databases
  ProtectionData _currentProt;
  uint64_t _currentVersion;  // This is the version in Current which underlies
                             // the data in _currentDatabases,
                             // _currentCollections and _shardsIds
  uint64_t _currentIndex;    // This is the Raft index, which corresponds to the
                             // above current version
  uint64_t
      _currentMemoryUsage;  // memory usage of VPack objects inside _current

  // We need information about collections, again we have
  // data from Plan and from Current.
  // The information for _shards and _shardKeys are filled from the
  // Plan (since they are fixed for the lifetime of the collection).
  // _shardIds is filled from Current, since we have to be able to
  // move shards between servers, and Plan contains who ought to be
  // responsible and Current contains the actual current responsibility.

  // The Plan state:
  AllCollections _plannedCollections;     // from Plan/Collections/
  AllCollections _newPlannedCollections;  // TODO
  // TODO is it ok to don't account value for _shards?
  FlatMapShared<pmr::CollectionID, std::vector<ShardID> const>
      _shards;  // from Plan/Collections/
                // (may later come from Current/Collections/ )
  // planned shard => servers map
  FlatMapShared<ShardID, ManagedVector<pmr::ServerID> const>
      _shardsToPlanServers;
  // planned shard ID => collection name
  FlatMap<ShardID, pmr::CollectionID> _shardToName;
  // name of the database a certain shard is part of
  FlatMap<ShardID, pmr::DatabaseID> _shardToDb;

  // planned shard ID => shard ID of shard group leader
  // This deserves an explanation. If collection B has `distributeShardsLike`
  // collection A, then A and B have the same number of shards. We say that
  // the k-th shard of A and the k-th shard of B are in the same "shard group".
  // This can be true for multiple collections, but they must then always
  // have the same collection A under `distributeShardsLike`. The shard of
  // collection A is then called the "shard group leader". It is guaranteed that
  // the shards of a shard group are always planned to be on the same
  // dbserver, and the leader is always the same for all shards in the group.
  // If a shard is a shard group leader, it does not appear in this map.
  // Example:
  //        Collection:      A        B        C
  //        Shard index 0:   s1       s5       s9
  //        Shard index 1:   s2       s6       s10
  //        Shard index 2:   s3       s7       s11
  //        Shard index 3:   s4       s8       s12
  // Here, collection B has "distributeShardsLike" set to "A",
  //       collection C has "distributeShardsLike" set to "B",
  //       the `numberOfShards` is 4 for all three collections.
  // Shard groups are: s1, s5, s9
  //              and: s2, s6, s10
  //              and: s3, s7, s11
  //              and: s4, s8, s12
  // Shard group leaders are s1, s2, s3 and s4.
  // That is, "shard group" is across collections, "shard index" is
  // within a collection.
  // All three collections must have the same `replicationFactor`, and
  // it is guaranteed, that all shards in a group always have the same
  // leader and the same list of followers.
  // Note however, that a follower for a shard group can be in sync with
  // its leader for some of the shards in the group and not for others!
  // Note that shard group leaders themselves do not appear in this map:
  FlatMap<ShardID, ShardID> _shardToShardGroupLeader;
  // In the following map we store for each shard group leader the list
  // of shards in the group, including the leader.
  FlatMapShared<ShardID, ManagedVector<ShardID>> _shardGroups;

  AllViews _plannedViews;     // from Plan/Views/
  AllViews _newPlannedViews;  // views that have been created during `loadPlan`
                              // execution

  // database ID => analyzers revision
  FlatMapShared<pmr::DatabaseID, AnalyzersRevision const>
      _dbAnalyzersRevision;  // from Plan/Analyzers

  std::atomic<std::thread::id> _planLoader;  // thread id that is loading plan

  // The Current state:
  FlatMapShared<pmr::DatabaseID, DatabaseCollectionsCurrent const>
      _currentCollections;  // from Current/Collections/
  FlatMapShared<ShardID, ManagedVector<pmr::ServerID> const>
      _shardsToCurrentServers;  // from Current/Collections/

  struct NewStuffByDatabase;
  FlatMapShared<pmr::DatabaseID, NewStuffByDatabase const> _newStuffByDatabase;

  using ReplicatedLogsMap =
      FlatMapShared<replication2::LogId,
                    replication2::agency::LogPlanSpecification const>;
  // note: protected by _planProt!
  ReplicatedLogsMap _replicatedLogs;

  using CollectionGroupMap = FlatMapShared<
      replication2::agency::CollectionGroupId,
      replication2::agency::CollectionGroupPlanSpecification const>;
  // note: protected by _planProt
  CollectionGroupMap _collectionGroups;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief uniqid sequence
  //////////////////////////////////////////////////////////////////////////////

  struct {
    uint64_t _currentValue;
    uint64_t _upperValue;
    uint64_t _nextBatchStart;
    uint64_t _nextUpperValue;
    bool _backgroundJobIsRunning;
  } _uniqid;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lock for uniqid sequence
  //////////////////////////////////////////////////////////////////////////////

  std::mutex _idLock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief how big a batch is for unique ids
  //////////////////////////////////////////////////////////////////////////////

  static constexpr uint64_t MinIdsPerBatch = 1000000;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check analyzers precondition timeout in seconds
  //////////////////////////////////////////////////////////////////////////////

  static constexpr double checkAnalyzersPreconditionTimeout = 10.0;

  mutable std::mutex _failedServersMutex;
  template<typename K>
  using HashSet =
      containers::FlatHashSet<K, typename containers::FlatHashSet<K>::hasher,
                              typename containers::FlatHashSet<K>::key_equal,
                              ClusterInfoResourceAllocator<K>>;
  HashSet<pmr::ServerID> _failedServers;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief plan and current update threads
  //////////////////////////////////////////////////////////////////////////////
  std::unique_ptr<SyncerThread> _planSyncer;
  std::unique_ptr<SyncerThread> _curSyncer;

  mutable std::mutex _waitPlanLock;
  AssocMultiMap<uint64_t, futures::Promise<Result>> _waitPlan;
  mutable std::mutex _waitPlanVersionLock;
  AssocMultiMap<consensus::index_t, futures::Promise<Result>> _waitPlanVersion;
  mutable std::mutex _waitCurrentLock;
  AssocMultiMap<uint64_t, futures::Promise<Result>> _waitCurrent;
  mutable std::mutex _waitCurrentVersionLock;
  AssocMultiMap<consensus::index_t, futures::Promise<Result>>
      _waitCurrentVersion;
};

namespace cluster {

// Note that while a network error will just return a failed `ResultT`, there
// are still possible exceptions.
futures::Future<ResultT<uint64_t>> fetchPlanVersion(network::Timeout timeout,
                                                    bool skipScheduler);
futures::Future<ResultT<uint64_t>> fetchCurrentVersion(network::Timeout timeout,
                                                       bool skipScheduler);

}  // namespace cluster
}  // namespace arangodb
