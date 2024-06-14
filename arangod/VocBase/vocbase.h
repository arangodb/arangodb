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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Basics/voc-errors.h"
#include "Containers/FlatHashMap.h"
#include "Replication2/Version.h"
#include "RestServer/arangod.h"
#include "Utils/VersionTracker.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace aql {
class QueryList;
}
namespace replication2 {
class LogId;
struct LogIndex;
struct LogTerm;
struct LogPayload;
namespace agency {
struct LogPlanSpecification;
struct LogPlanTermSpecification;
struct ParticipantsConfig;
}  // namespace agency
namespace maintenance {
struct LogStatus;
}
namespace replicated_log {
struct ILogLeader;
struct ILogFollower;
struct ILogParticipant;
struct LogStatus;
struct QuickLogStatus;
struct ReplicatedLog;
}  // namespace replicated_log
namespace replicated_state {
struct ReplicatedStateBase;
struct StateStatus;
}  // namespace replicated_state
namespace storage {
struct PersistedStateInfo;
struct IStorageEngineMethods;
}  // namespace storage
}  // namespace replication2
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace futures {
template<typename T>
class Future;
}
class CursorRepository;
struct DatabaseConfiguration;
struct DatabaseJavaScriptCache;
class DatabaseReplicationApplier;
class LogicalCollection;
class LogicalDataSource;
class LogicalView;
struct CreateCollectionBody;
class ReplicationClientsProgressTracker;
class StorageEngine;
class VersionTracker;
struct VocBaseLogManager;
struct VocbaseMetrics;
}  // namespace arangodb

/// @brief document handle separator as character
inline constexpr char TRI_DOCUMENT_HANDLE_SEPARATOR_CHR = '/';

/// @brief document handle separator as string
inline constexpr auto TRI_DOCUMENT_HANDLE_SEPARATOR_STR = "/";

/// @brief index handle separator as character
inline constexpr char TRI_INDEX_HANDLE_SEPARATOR_CHR = '/';

/// @brief index handle separator as string
inline constexpr auto TRI_INDEX_HANDLE_SEPARATOR_STR = "/";

/// @brief database
struct TRI_vocbase_t {
  friend class arangodb::StorageEngine;

  explicit TRI_vocbase_t(arangodb::CreateDatabaseInfo&& info);

  // note: isInternal=true is currently only used for the special internal
  // vocbase object that is used to execute IResearchAqlAnalyzer computations,
  // namely calculationVocbase.
  // all other vocbases do not set it.
  // the isInternal flag is necessary for a slightly different setup of the
  // internal vocbase object, which does not use the MetricsFeature, because
  // it can outlive the entire ApplicationServer stack.
  TRI_vocbase_t(arangodb::CreateDatabaseInfo&& info,
                arangodb::VersionTracker& versionTracker, bool extendedNames,
                bool isInternal = false);
  TEST_VIRTUAL ~TRI_vocbase_t();

#ifdef ARANGODB_USE_GOOGLE_TESTS
 protected:
  struct MockConstruct {
  } constexpr static mockConstruct = {};
  TRI_vocbase_t(MockConstruct, arangodb::CreateDatabaseInfo&& info,
                arangodb::StorageEngine& engine,
                arangodb::VersionTracker& versionTracker, bool extendedNames);
#endif

 private:
  // explicitly document implicit behavior (due to presence of locks)
  TRI_vocbase_t(TRI_vocbase_t&&) = delete;
  TRI_vocbase_t(TRI_vocbase_t const&) = delete;
  TRI_vocbase_t& operator=(TRI_vocbase_t&&) = delete;
  TRI_vocbase_t& operator=(TRI_vocbase_t const&) = delete;

  arangodb::ArangodServer& _server;
  arangodb::StorageEngine& _engine;
  arangodb::VersionTracker& _versionTracker;
  bool const _extendedNames;  // TODO - move this into CreateDatabaseInfo

  arangodb::CreateDatabaseInfo _info;

  std::atomic_uint64_t _refCount{0};

  std::vector<std::shared_ptr<arangodb::LogicalCollection>>
      _collections;  // ALL collections
  std::vector<std::shared_ptr<arangodb::LogicalCollection>>
      _deadCollections;  // collections dropped that can be removed later

  arangodb::containers::FlatHashMap<
      arangodb::DataSourceId,
      std::shared_ptr<arangodb::LogicalDataSource>>
      _dataSourceById;  // data-source by id
  arangodb::containers::FlatHashMap<
      std::string, std::shared_ptr<arangodb::LogicalDataSource>>
      _dataSourceByName;  // data-source by name
  arangodb::containers::FlatHashMap<
      std::string, std::shared_ptr<arangodb::LogicalDataSource>>
      _dataSourceByUuid;  // data-source by uuid
  mutable arangodb::basics::ReadWriteLock
      _dataSourceLock;  // data-source iterator lock
  mutable std::atomic<std::thread::id>
      _dataSourceLockWriteOwner;  // current thread owning '_dataSourceLock'
                                  // write lock (workaround for non-recusrive
                                  // ReadWriteLock)

  std::unique_ptr<arangodb::aql::QueryList> _queries;
  std::unique_ptr<arangodb::CursorRepository> _cursorRepository;

  std::unique_ptr<arangodb::VocbaseMetrics> _metrics;

  std::unique_ptr<arangodb::DatabaseReplicationApplier> _replicationApplier;
  std::unique_ptr<arangodb::ReplicationClientsProgressTracker>
      _replicationClients;

 public:
  arangodb::StorageEngine& engine() const noexcept { return _engine; }

  auto extendedNames() const noexcept -> bool { return _extendedNames; }

  auto versionTracker() noexcept -> arangodb::VersionTracker& {
    return _versionTracker;
  }

  arangodb::VocbaseMetrics const& metrics() const noexcept { return *_metrics; }

  template<typename As>
  As& engine() const noexcept
      requires(std::derived_from<As, arangodb::StorageEngine>) {
    return static_cast<As&>(_engine);
  }

  std::shared_ptr<arangodb::VocBaseLogManager> _logManager;

 public:
  auto createReplicatedState(arangodb::replication2::LogId id,
                             std::string_view type,
                             arangodb::velocypack::Slice parameter)
      -> arangodb::ResultT<std::shared_ptr<
          arangodb::replication2::replicated_state::ReplicatedStateBase>>;
  auto dropReplicatedState(arangodb::replication2::LogId id) noexcept
      -> arangodb::Result;

  auto getReplicatedStateById(arangodb::replication2::LogId id)
      -> arangodb::ResultT<std::shared_ptr<
          arangodb::replication2::replicated_state::ReplicatedStateBase>>;

  auto updateReplicatedState(
      arangodb::replication2::LogId id,
      arangodb::replication2::agency::LogPlanTermSpecification const& spec,
      arangodb::replication2::agency::ParticipantsConfig const&)
      -> arangodb::Result;

  [[nodiscard]] auto getReplicatedLogsStatusMap() const
      -> std::unordered_map<arangodb::replication2::LogId,
                            arangodb::replication2::maintenance::LogStatus>;

  [[nodiscard]] auto getReplicatedStatesStatus() const
      -> std::unordered_map<arangodb::replication2::LogId,
                            arangodb::replication2::replicated_log::LogStatus>;

 public:
  // Old Replicated Log API. Still there for compatibility. To be removed.
  auto getReplicatedLogById(arangodb::replication2::LogId id)
      -> std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog>;
  auto getReplicatedLogLeaderById(arangodb::replication2::LogId id)
      -> std::shared_ptr<arangodb::replication2::replicated_log::ILogLeader>;
  auto getReplicatedLogFollowerById(arangodb::replication2::LogId id)
      -> std::shared_ptr<arangodb::replication2::replicated_log::ILogFollower>;

  void shutdownReplicatedLogs() noexcept;
  void dropReplicatedLogs() noexcept;

  [[nodiscard]] auto getDatabaseConfiguration()
      -> arangodb::DatabaseConfiguration;

 public:
  arangodb::basics::ReadWriteLock _inventoryLock;  // object lock needed when
                                                   // replication is assessing
                                                   // the state of the vocbase

  // structures for volatile cache data (used from JavaScript)
#ifdef USE_V8
  std::unique_ptr<arangodb::DatabaseJavaScriptCache> _cacheData;
#endif

  arangodb::ArangodServer& server() const noexcept { return _server; }

  TRI_voc_tick_t id() const { return _info.getId(); }
  decltype(auto) name() const { return _info.getName(); }
  std::string path() const;
  std::uint32_t replicationFactor() const;
  std::uint32_t writeConcern() const;
  arangodb::replication::Version replicationVersion() const;
  decltype(auto) sharding() const { return _info.sharding(); }
  bool isOneShard() const;

  void toVelocyPack(arangodb::velocypack::Builder& result) const;
  arangodb::ReplicationClientsProgressTracker& replicationClients() {
    return *_replicationClients;
  }

  arangodb::DatabaseReplicationApplier* replicationApplier() const {
    return _replicationApplier.get();
  }
  void addReplicationApplier();

  arangodb::aql::QueryList* queryList() const { return _queries.get(); }
  arangodb::CursorRepository* cursorRepository() const {
    return _cursorRepository.get();
  }

  /// @brief increase the reference counter for a database.
  /// will return true if the reference counter was increased, false otherwise
  /// in case false is returned, the database must not be used
  bool use() noexcept;

  void forceUse() noexcept;

  /// @brief decrease the reference counter for a database
  void release() noexcept;

  /// @brief returns whether the database is dangling
  bool isDangling() const noexcept;

  /// @brief whether or not the vocbase has been marked as deleted
  bool isDropped() const noexcept;

  /// @brief marks a database as deleted
  bool markAsDropped() noexcept;

  /// @brief returns whether the database is the system database
  bool isSystem() const noexcept;

  /// @brief stop operations in this vocbase. must be called prior to
  /// shutdown to clean things up
  void stop();

  /// @brief closes a database and all collections
  void shutdown();

  /// @brief sets prototype collection for sharding (_users or _graphs)
  void setShardingPrototype(ShardingPrototype type);

  /// @brief sets sharding property for sharding (e.g. single, flexible);
  void setSharding(std::string_view sharding);

  /// @brief gets prototype collection for sharding (_users or _graphs)
  ShardingPrototype shardingPrototype() const;

  /// @brief gets name of prototype collection for sharding (_users or _graphs)
  std::string const& shardingPrototypeName() const;

  /// @brief returns all known views
  std::vector<std::shared_ptr<arangodb::LogicalView>> views() const;

  /// @brief returns all known collections
  std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections(
      bool includeDeleted) const;

  void processCollectionsOnShutdown(
      std::function<void(arangodb::LogicalCollection*)> const& cb);

  void processCollections(
      std::function<void(arangodb::LogicalCollection*)> const& cb);

  /// @brief returns names of all known collections
  std::vector<std::string> collectionNames() const;

  /// @brief creates a new view from parameter set
  std::shared_ptr<arangodb::LogicalView> createView(
      arangodb::velocypack::Slice parameters, bool isUserRequest);

  /// @brief drops a view
  arangodb::Result dropView(arangodb::DataSourceId cid, bool allowDropSystem);

  /// @brief returns all known collections with their parameters and indexes,
  /// up to a specific tick value
  /// while the collections are iterated over, there will be a global lock so
  /// that there will be consistent view of collections & their properties
  /// The list of collections will be sorted by type and name (vertices before
  /// edges)
  void inventory(arangodb::velocypack::Builder& result, TRI_voc_tick_t,
                 std::function<bool(arangodb::LogicalCollection const*)> const&
                     nameFilter);

  /// @brief looks up a collection by identifier
  std::shared_ptr<arangodb::LogicalCollection> lookupCollection(
      arangodb::DataSourceId id) const noexcept;

  /// @brief looks up a collection by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalCollection> lookupCollection(
      std::string_view nameOrId) const noexcept;

  /// @brief looks up a collection by uuid
  std::shared_ptr<arangodb::LogicalCollection> lookupCollectionByUuid(
      std::string_view uuid) const noexcept;

  /// @brief looks up a data-source by identifier
  std::shared_ptr<arangodb::LogicalDataSource> lookupDataSource(
      arangodb::DataSourceId id) const noexcept;

  /// @brief looks up a data-source by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalDataSource> lookupDataSource(
      std::string_view nameOrId) const noexcept;

  /// @brief looks up a replicated log by identifier
  std::shared_ptr<arangodb::replication2::replicated_log::ILogParticipant>
  lookupLog(arangodb::replication2::LogId id) const noexcept;

  /// @brief looks up a view by identifier
  std::shared_ptr<arangodb::LogicalView> lookupView(
      arangodb::DataSourceId id) const;

  /// @brief looks up a view by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalView> lookupView(
      std::string_view nameOrId) const;

  /// @brief renames a collection
  arangodb::Result renameCollection(arangodb::DataSourceId cid,
                                    std::string_view newName);

  /// @brief renames a view
  arangodb::Result renameView(arangodb::DataSourceId cid,
                              std::string_view oldName);

  /// @brief creates an array of new collections from parameter set. the
  /// input slice must be an array of collection description objects.
  /// all collection descriptions are validated first. upon validation error,
  /// an exception is thrown. if all collection descriptions have passed
  /// validation, the collection objects will be created and registered,
  /// so that the collections can be looked up and found by name, guid etc.
  /// if creating or registering any of the collections fails after the
  /// initial validation, any already created collections are not deleted
  /// (i.e. no rollback if only some of the collections can be created or
  /// registered after initial validation).
  std::vector<std::shared_ptr<arangodb::LogicalCollection>> createCollections(
      arangodb::velocypack::Slice infoSlice,
      bool allowEnterpriseCollectionsOnSingleServer);

  [[nodiscard]] arangodb::ResultT<
      std::vector<std::shared_ptr<arangodb::LogicalCollection>>>
  createCollections(std::vector<arangodb::CreateCollectionBody> const&
                        parametersOfCollections,
                    bool allowEnterpriseCollectionsOnSingleServer);

  /// @brief creates a new collection from parameter set
  /// collection id ("cid") is normally passed with a value of 0
  /// this means that the system will assign a new collection id automatically
  /// using a cid of > 0 is supported to import dumps from other servers etc.
  /// but the functionality is not advertised
  std::shared_ptr<arangodb::LogicalCollection> createCollection(
      arangodb::velocypack::Slice parameters);

  /// @brief drops a collection.
  arangodb::Result dropCollection(arangodb::DataSourceId cid,
                                  bool allowDropSystem);

  /// @brief validate parameters for collection creation.
  arangodb::Result validateCollectionParameters(
      arangodb::velocypack::Slice parameters);

  /// @brief locks a collection for usage by id.
  /// note: when the collection is not used anymore, the caller *must*
  /// call vocbase::releaseCollection() to decrease the reference
  /// counter for this collection
  std::shared_ptr<arangodb::LogicalCollection> useCollection(
      arangodb::DataSourceId cid, bool checkPermissions);

  /// @brief locks a collection for usage by name.
  /// note: when the collection is not used anymore, the caller *must*
  /// call vocbase::releaseCollection() to decrease the reference
  /// counter for this collection
  std::shared_ptr<arangodb::LogicalCollection> useCollection(
      std::string_view name, bool checkPermissions);

  /// @brief releases a collection from usage
  void releaseCollection(arangodb::LogicalCollection* collection) noexcept;

  /// @brief creates a collection object (of type LogicalCollection or one of
  /// the SmartGraph-specific subtypes). the object only exists on the heap and
  /// is not yet persisted anywhere. note: this should only be called for
  /// valid collection definitions (i.e. validation should be done before!).
  /// the method will throw if the collection cannot be created.
  /// the isAStub flag should be set to true for collections created by
  /// ClusterInfo.
  std::shared_ptr<arangodb::LogicalCollection> createCollectionObject(
      arangodb::velocypack::Slice data, bool isAStub);

  /// @brief creates a collection object (of type LogicalCollection or one of
  /// the SmartGraph-specific subtypes) for storage. The object is augmented
  /// with storage engine-specific data (e.g. objectId). the object only exists
  /// on the heap and is not yet persisted anywhere. note: this should only be
  /// called for valid collection definitions (i.e. validation should be done
  /// before!) and not on coordinators (coordinators are not expected to store
  /// any collections).
  std::shared_ptr<arangodb::LogicalCollection> createCollectionObjectForStorage(
      arangodb::velocypack::Slice parameters);

  /// @brief callback for collection dropping
  static bool dropCollectionCallback(arangodb::LogicalCollection& collection);

 private:
  /// @brief adds further SmartGraph-specific sub-collections to the vector of
  /// collections if collection is a SmartGraph edge collection that requires
  /// it. otherwise does nothing.
  void addSmartGraphCollections(
      std::shared_ptr<arangodb::LogicalCollection> const& collection,
      std::vector<std::shared_ptr<arangodb::LogicalCollection>>& collections)
      const;

  /// @brief validates SmartGraph-specific collection parameters. does nothing
  /// in community edition or if the collection is not a SmartGraph collection.
  arangodb::Result validateExtendedCollectionParameters(
      arangodb::velocypack::Slice parameters);

  /// @brief stores the collection object in the list of available collections,
  /// so it can later be looked up and found by name, guid etc.
  void persistCollection(
      std::shared_ptr<arangodb::LogicalCollection> const& collection);

  /// @brief check some invariants on the various lists of collections
  void checkCollectionInvariants() const noexcept;

  std::shared_ptr<arangodb::LogicalCollection> useCollectionInternal(
      std::shared_ptr<arangodb::LogicalCollection> const& collection,
      bool checkPermissions);

  /// @brief loads an existing collection
  /// Note that this will READ lock the collection. You have to release the
  /// collection lock by yourself.
  arangodb::Result loadCollection(arangodb::LogicalCollection& collection,
                                  bool checkPermissions);

  /// @brief adds a new collection
  /// caller must hold _dataSourceLock in write mode or set doLock
  void registerCollection(
      bool doLock,
      std::shared_ptr<arangodb::LogicalCollection> const& collection);

  /// @brief removes a collection from the global list of collections
  /// This function is called when a collection is dropped.
  /// NOTE: You need a writelock on _dataSourceLock
  void unregisterCollection(arangodb::LogicalCollection& collection);

  /// @brief drops a collection, worker function
  arangodb::Result dropCollectionWorker(
      arangodb::LogicalCollection& collection);

  /// @brief adds a new view
  /// caller must hold _dataSourceLock in write mode or set doLock
  void registerView(bool doLock,
                    std::shared_ptr<arangodb::LogicalView> const& view);

  /// @brief removes a view from the global list of views
  /// This function is called when a view is dropped.
  /// NOTE: You need a writelock on _dataSourceLock
  bool unregisterView(arangodb::LogicalView const& view);

  /// @brief adds a new replicated state given its log id and providing
  /// methods to access the storage engine
  void registerReplicatedState(
      arangodb::replication2::LogId,
      std::unique_ptr<arangodb::replication2::storage::IStorageEngineMethods>);
};

/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open
/// the result is the object excluding _id, _key and _rev
void TRI_SanitizeObject(arangodb::velocypack::Slice slice,
                        arangodb::velocypack::Builder& builder);
