////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Basics/Common.h"
#include "Basics/DeadlockDetector.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Basics/voc-errors.h"
#include "Replication2/Version.h"
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
namespace replicated_log {
class LogLeader;
class LogFollower;
struct ILogParticipant;
struct LogStatus;
struct PersistedLog;
struct ReplicatedLog;
}  // namespace replicated_log
}  // namespace replication2
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class CursorRepository;
struct DatabaseJavaScriptCache;
class DatabaseReplicationApplier;
class LogicalCollection;
class LogicalDataSource;
class LogicalView;
class ReplicationClientsProgressTracker;
class StorageEngine;
struct VocBaseLogManager;
}  // namespace arangodb

/// @brief document handle separator as character
constexpr char TRI_DOCUMENT_HANDLE_SEPARATOR_CHR = '/';

/// @brief document handle separator as string
constexpr auto TRI_DOCUMENT_HANDLE_SEPARATOR_STR = "/";

/// @brief index handle separator as character
constexpr char TRI_INDEX_HANDLE_SEPARATOR_CHR = '/';

/// @brief index handle separator as string
constexpr auto TRI_INDEX_HANDLE_SEPARATOR_STR = "/";

/// @brief database
struct TRI_vocbase_t {
  friend class arangodb::StorageEngine;

  TRI_vocbase_t(TRI_vocbase_type_e type, arangodb::CreateDatabaseInfo&&);
  TEST_VIRTUAL ~TRI_vocbase_t();
  
 private:
  // explicitly document implicit behavior (due to presence of locks)
  TRI_vocbase_t(TRI_vocbase_t&&) = delete;
  TRI_vocbase_t(TRI_vocbase_t const&) = delete;
  TRI_vocbase_t& operator=(TRI_vocbase_t&&) = delete;
  TRI_vocbase_t& operator=(TRI_vocbase_t const&) = delete;

  /// @brief sleep interval used when polling for a loading collection's status
  static constexpr unsigned collectionStatusPollInterval() { return 10 * 1000; }

  /// @brief states for dropping
  enum DropState {
    DROP_EXIT,    // drop done, nothing else to do
    DROP_AGAIN,   // drop not done, must try again
    DROP_PERFORM  // drop done, must perform actual cleanup routine
  };

  arangodb::application_features::ApplicationServer& _server;

  arangodb::CreateDatabaseInfo _info;

  TRI_vocbase_type_e _type;  // type (normal or coordinator)
  std::atomic<uint64_t> _refCount;
  bool _isOwnAppsDirectory;

  std::vector<std::shared_ptr<arangodb::LogicalCollection>> _collections;  // ALL collections
  std::vector<std::shared_ptr<arangodb::LogicalCollection>> _deadCollections;  // collections dropped that can be removed later

  std::unordered_map<arangodb::DataSourceId,
                     std::shared_ptr<arangodb::LogicalDataSource>>
      _dataSourceById;  // data-source by id
  std::unordered_map<std::string, std::shared_ptr<arangodb::LogicalDataSource>> _dataSourceByName;  // data-source by name
  std::unordered_map<std::string, std::shared_ptr<arangodb::LogicalDataSource>> _dataSourceByUuid;  // data-source by uuid
  mutable arangodb::basics::ReadWriteLock _dataSourceLock;  // data-source iterator lock
  mutable std::atomic<std::thread::id> _dataSourceLockWriteOwner;  // current thread owning '_dataSourceLock'
                                                                   // write lock (workaround for non-recusrive
                                                                   // ReadWriteLock)

  std::unique_ptr<arangodb::aql::QueryList> _queries;
  std::unique_ptr<arangodb::CursorRepository> _cursorRepository;

  std::unique_ptr<arangodb::DatabaseReplicationApplier> _replicationApplier;
  std::unique_ptr<arangodb::ReplicationClientsProgressTracker> _replicationClients;

 public:
  std::shared_ptr<arangodb::VocBaseLogManager> _logManager;
  [[nodiscard]] auto getReplicatedLogById(arangodb::replication2::LogId id) const
      -> std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog>;
  [[nodiscard]] auto getReplicatedLogLeaderById(arangodb::replication2::LogId id) const
      -> std::shared_ptr<arangodb::replication2::replicated_log::LogLeader>;
  [[nodiscard]] auto getReplicatedLogFollowerById(arangodb::replication2::LogId id) const
      -> std::shared_ptr<arangodb::replication2::replicated_log::LogFollower>;
  [[nodiscard]] auto getReplicatedLogs() const
      -> std::unordered_map<arangodb::replication2::LogId, arangodb::replication2::replicated_log::LogStatus>;
  [[nodiscard]] auto createReplicatedLog(arangodb::replication2::LogId id,
                                         std::optional<std::string> const& collectionName)
      -> arangodb::ResultT<std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog>>;
  [[nodiscard]] auto dropReplicatedLog(arangodb::replication2::LogId id) -> arangodb::Result;
  auto ensureReplicatedLog(arangodb::replication2::LogId id,
                           std::optional<std::string> const& collectionName)
      -> std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog>;

 public:
  arangodb::basics::DeadlockDetector<arangodb::TransactionId, arangodb::LogicalCollection> _deadlockDetector;
  arangodb::basics::ReadWriteLock _inventoryLock;  // object lock needed when
                                                   // replication is assessing
                                                   // the state of the vocbase

  // structures for volatile cache data (used from JavaScript)
  std::unique_ptr<arangodb::DatabaseJavaScriptCache> _cacheData;

  arangodb::application_features::ApplicationServer& server() const noexcept {
    return _server;
  }

  TRI_voc_tick_t id() const { return _info.getId(); }
  std::string const& name() const { return _info.getName(); }
  std::string path() const;
  std::uint32_t replicationFactor() const;
  std::uint32_t writeConcern() const;
  arangodb::replication::Version replicationVersion() const;
  std::string const& sharding() const;
  bool isOneShard() const;
  TRI_vocbase_type_e type() const { return _type; }

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

  bool isOwnAppsDirectory() const { return _isOwnAppsDirectory; }
  void setIsOwnAppsDirectory(bool value) { _isOwnAppsDirectory = value; }

  /// @brief increase the reference counter for a database.
  /// will return true if the refeence counter was increased, false otherwise
  /// in case false is returned, the database must not be used
  bool use();

  void forceUse();

  /// @brief decrease the reference counter for a database
  void release() noexcept;

  /// @brief returns whether the database is dangling
  bool isDangling() const;

  /// @brief whether or not the vocbase has been marked as deleted
  bool isDropped() const;

  /// @brief marks a database as deleted
  bool markAsDropped();

  /// @brief returns whether the database is the system database
  bool isSystem() const;

  /// @brief stop operations in this vocbase. must be called prior to
  /// shutdown to clean things up
  void stop();

  /// @brief closes a database and all collections
  void shutdown();

  /// @brief sets prototype collection for sharding (_users or _graphs)
  void setShardingPrototype(ShardingPrototype type);

  /// @brief gets prototype collection for sharding (_users or _graphs)
  ShardingPrototype shardingPrototype() const;
 
  /// @brief gets name of prototype collection for sharding (_users or _graphs)
  std::string const& shardingPrototypeName() const;

  /// @brief returns all known views
  std::vector<std::shared_ptr<arangodb::LogicalView>> views();

  /// @brief returns all known collections
  std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections(bool includeDeleted);

  void processCollectionsOnShutdown(std::function<void(arangodb::LogicalCollection*)> const& cb);

  void processCollections(std::function<void(arangodb::LogicalCollection*)> const& cb);

  /// @brief returns names of all known collections
  std::vector<std::string> collectionNames();

  /// @brief creates a new view from parameter set
  std::shared_ptr<arangodb::LogicalView> createView(arangodb::velocypack::Slice parameters);

  /// @brief drops a view
  arangodb::Result dropView(arangodb::DataSourceId cid, bool allowDropSystem);

  /// @brief returns all known collections with their parameters
  /// and optionally indexes
  /// the result is sorted by type and name (vertices before edges)
  void inventory(arangodb::velocypack::Builder& result, TRI_voc_tick_t,
                 std::function<bool(arangodb::LogicalCollection const*)> const& nameFilter);

  /// @brief looks up a collection by identifier
  std::shared_ptr<arangodb::LogicalCollection> lookupCollection(arangodb::DataSourceId id) const
      noexcept;

  /// @brief looks up a collection by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalCollection> lookupCollection(std::string const& nameOrId) const
      noexcept;

  /// @brief looks up a collection by uuid
  std::shared_ptr<arangodb::LogicalCollection> lookupCollectionByUuid(std::string const& uuid) const
      noexcept;

  /// @brief looks up a data-source by identifier
  std::shared_ptr<arangodb::LogicalDataSource> lookupDataSource(arangodb::DataSourceId id) const
      noexcept;

  /// @brief looks up a data-source by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalDataSource> lookupDataSource(std::string const& nameOrId) const
      noexcept;

  /// @brief looks up a replicated log by identifier
  std::shared_ptr<arangodb::replication2::replicated_log::ILogParticipant> lookupLog(
      arangodb::replication2::LogId id) const noexcept;

  /// @brief looks up a view by identifier
  std::shared_ptr<arangodb::LogicalView> lookupView(arangodb::DataSourceId id) const;

  /// @brief looks up a view by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalView> lookupView(std::string const& nameOrId) const;

  /// @brief renames a collection
  arangodb::Result renameCollection(arangodb::DataSourceId cid, std::string const& newName);

  /// @brief renames a view
  arangodb::Result renameView(arangodb::DataSourceId cid, std::string const& oldName);

  /// @brief creates a new collection from parameter set
  /// collection id ("cid") is normally passed with a value of 0
  /// this means that the system will assign a new collection id automatically
  /// using a cid of > 0 is supported to import dumps from other servers etc.
  /// but the functionality is not advertised
  std::shared_ptr<arangodb::LogicalCollection> createCollection(arangodb::velocypack::Slice parameters);

  /// @brief drops a collection, no timeout if timeout is < 0.0, otherwise
  /// timeout is in seconds. Essentially, the timeout counts to acquire the
  /// write lock for using the collection.
  arangodb::Result dropCollection(arangodb::DataSourceId cid,
                                  bool allowDropSystem, double timeout);

  /// @brief locks a collection for usage by id
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
  /// when you are done with the collection.
  std::shared_ptr<arangodb::LogicalCollection> useCollection(arangodb::DataSourceId cid,
                                                             bool checkPermissions);

  /// @brief locks a collection for usage by name
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
  /// when you are done with the collection.
  std::shared_ptr<arangodb::LogicalCollection> useCollection(std::string const& name, bool checkPermissions);

  /// @brief releases a collection from usage
  void releaseCollection(arangodb::LogicalCollection* collection) noexcept;

  /// @brief visit all DataSources registered with this vocbase
  /// @param visitor returns if visitation should continue
  /// @param lockWrite acquire write lock (if 'visitor' will modify vocbase)
  /// @return visitation compleated successfully
  typedef std::function<bool(arangodb::LogicalDataSource& dataSource)> dataSourceVisitor;
  bool visitDataSources(dataSourceVisitor const& visitor);

  std::shared_ptr<arangodb::LogicalCollection> persistCollection(std::shared_ptr<arangodb::LogicalCollection>& collection);

 private:
  /// @brief callback for collection dropping
  static bool dropCollectionCallback(arangodb::LogicalCollection& collection);

  /// @brief check some invariants on the various lists of collections
  void checkCollectionInvariants() const;

  std::shared_ptr<arangodb::LogicalCollection> useCollectionInternal(
      std::shared_ptr<arangodb::LogicalCollection> const&, bool checkPermissions);

  arangodb::Result loadCollection(arangodb::LogicalCollection& collection,
                                  bool checkPermissions);

  /// @brief adds a new collection
  /// caller must hold _dataSourceLock in write mode or set doLock
  void registerCollection(bool doLock, std::shared_ptr<arangodb::LogicalCollection> const& collection);

  /// @brief removes a collection from the global list of collections
  /// This function is called when a collection is dropped.
  void unregisterCollection(arangodb::LogicalCollection& collection);

  /// @brief creates a new collection, worker function
  std::shared_ptr<arangodb::LogicalCollection> createCollectionWorker(arangodb::velocypack::Slice parameters);

  /// @brief drops a collection, worker function
  ErrorCode dropCollectionWorker(arangodb::LogicalCollection* collection,
                                 DropState& state, double timeout);

  /// @brief adds a new view
  /// caller must hold _dataSourceLock in write mode or set doLock
  void registerView(bool doLock, std::shared_ptr<arangodb::LogicalView> const& view);

  /// @brief removes a view from the global list of views
  /// This function is called when a view is dropped.
  bool unregisterView(arangodb::LogicalView const& view);

  /// @brief adds a new replicated log with given log id
  void registerReplicatedLog(arangodb::replication2::LogId, std::shared_ptr<arangodb::replication2::replicated_log::PersistedLog>);

  /// @brief removes the replicated log with the given id
  void unregisterReplicatedLog(arangodb::replication2::LogId);
};

/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open
void TRI_SanitizeObject(arangodb::velocypack::Slice const slice,
                        arangodb::velocypack::Builder& builder);
void TRI_SanitizeObjectWithEdges(arangodb::velocypack::Slice const slice,
                                 arangodb::velocypack::Builder& builder);

