////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOC_BASE_VOCBASE_H
#define ARANGOD_VOC_BASE_VOCBASE_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/DeadlockDetector.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/StringUtils.h"
#include "Basics/voc-errors.h"
#include "VocBase/voc-types.h"

#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

#include <functional>

namespace arangodb {
namespace aql {
class QueryList;
}
class CollectionNameResolver;
class CollectionKeysRepository;
class CursorRepository;
class DatabaseReplicationApplier;
class LogicalCollection;
class LogicalDataSource;
class LogicalView;
class StorageEngine;
}

/// @brief predefined collection name for users
constexpr auto TRI_COL_NAME_USERS = "_users";

/// @brief name of the system database
constexpr auto TRI_VOC_SYSTEM_DATABASE = "_system";

/// @brief maximal name length
constexpr size_t TRI_COL_NAME_LENGTH = 64;

/// @brief default maximal collection journal size
constexpr size_t TRI_JOURNAL_DEFAULT_SIZE = 1024 * 1024 * 32;  // 32 MiB

////////////////////////////////////////////////////////////////////////////////
/// @brief minimal collection journal size (for testing, we allow very small
/// file sizes in maintainer mode)
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

constexpr size_t TRI_JOURNAL_MINIMAL_SIZE = 16 * 1024;  // 16 KiB

#else

constexpr size_t TRI_JOURNAL_MINIMAL_SIZE = 1024 * 1024;  // 1 MiB

#endif

/// @brief document handle separator as character
constexpr char TRI_DOCUMENT_HANDLE_SEPARATOR_CHR = '/';

/// @brief document handle separator as string
constexpr auto TRI_DOCUMENT_HANDLE_SEPARATOR_STR = "/";

/// @brief index handle separator as character
constexpr char TRI_INDEX_HANDLE_SEPARATOR_CHR = '/';

/// @brief index handle separator as string
constexpr auto TRI_INDEX_HANDLE_SEPARATOR_STR = "/";

/// @brief collection enum
enum TRI_col_type_e : uint32_t {
  TRI_COL_TYPE_UNKNOWN = 0,  // only used to signal an invalid collection type
  TRI_COL_TYPE_DOCUMENT = 2,
  TRI_COL_TYPE_EDGE = 3
};

/// @brief database type
enum TRI_vocbase_type_e {
  TRI_VOCBASE_TYPE_NORMAL = 0,
  TRI_VOCBASE_TYPE_COORDINATOR = 1
};

/// @brief status of a collection
/// note: the NEW_BORN status is not used in ArangoDB 1.3 anymore, but is left
/// in this enum for compatibility with earlier versions
enum TRI_vocbase_col_status_e : int {
  TRI_VOC_COL_STATUS_CORRUPTED = 0,
  TRI_VOC_COL_STATUS_NEW_BORN = 1,  // DEPRECATED, and shouldn't be used anymore
  TRI_VOC_COL_STATUS_UNLOADED = 2,
  TRI_VOC_COL_STATUS_LOADED = 3,
  TRI_VOC_COL_STATUS_UNLOADING = 4,
  TRI_VOC_COL_STATUS_DELETED = 5,
  TRI_VOC_COL_STATUS_LOADING = 6
};

/// @brief database
struct TRI_vocbase_t {
  friend class arangodb::StorageEngine;

  /// @brief database state
  enum class State {
    NORMAL = 0,
    SHUTDOWN_COMPACTOR = 1,
    SHUTDOWN_CLEANUP = 2
  };

  TRI_vocbase_t(TRI_vocbase_type_e type, TRI_voc_tick_t id,
                std::string const& name);
  TEST_VIRTUAL ~TRI_vocbase_t();

 private:
  // explicitly document implicit behaviour (due to presence of locks)
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

  TRI_voc_tick_t const _id;  // internal database id
  std::string _name;         // database name
  TRI_vocbase_type_e _type;  // type (normal or coordinator)
  std::atomic<uint64_t> _refCount;
  State _state;
  bool _isOwnAppsDirectory;

  std::vector<std::shared_ptr<arangodb::LogicalCollection>> _collections; // ALL collections
  std::vector<std::shared_ptr<arangodb::LogicalCollection>> _deadCollections; // collections dropped that can be removed later

  std::unordered_map<TRI_voc_cid_t, std::shared_ptr<arangodb::LogicalDataSource>> _dataSourceById; // data-source by id
  std::unordered_map<std::string, std::shared_ptr<arangodb::LogicalDataSource>> _dataSourceByName; // data-source by name
  std::unordered_map<std::string, std::shared_ptr<arangodb::LogicalDataSource>> _dataSourceByUuid; // data-source by uuid
  mutable arangodb::basics::ReadWriteLock _dataSourceLock; // data-source iterator lock
  mutable std::atomic<std::thread::id> _dataSourceLockWriteOwner; // current thread owning '_dataSourceLock' write lock (workaround for non-recusrive ReadWriteLock)

  std::unique_ptr<arangodb::aql::QueryList> _queries;
  std::unique_ptr<arangodb::CursorRepository> _cursorRepository;
  std::unique_ptr<arangodb::CollectionKeysRepository> _collectionKeys;

  std::unique_ptr<arangodb::DatabaseReplicationApplier> _replicationApplier;

  arangodb::basics::ReadWriteLock _replicationClientsLock;
  std::unordered_map<TRI_server_id_t, std::tuple<double, double, TRI_voc_tick_t>>
      _replicationClients;

 public:
  arangodb::basics::DeadlockDetector<TRI_voc_tid_t, arangodb::LogicalCollection>
      _deadlockDetector;
  arangodb::basics::ReadWriteLock _inventoryLock;  // object lock needed when
                                                   // replication is assessing
                                                   // the state of the vocbase

  // structures for user-defined volatile data
  void* _userStructures;

 public:
  /// @brief checks if a database name is allowed
  /// returns true if the name is allowed and false otherwise
  static bool IsAllowedName(arangodb::velocypack::Slice slice) noexcept;
  static bool IsAllowedName(
    bool allowSystem,
    arangodb::velocypack::StringRef const& name
  ) noexcept;

  /// @brief determine whether a data-source name is a system data-source name
  static bool IsSystemName(std::string const& name) noexcept;

  TRI_voc_tick_t id() const { return _id; }
  std::string const& name() const { return _name; }
  std::string path() const;
  TRI_vocbase_type_e type() const { return _type; }
  State state() const { return _state; }
  void setState(State state) { _state = state; }
  // return all replication clients registered
  std::vector<std::tuple<TRI_server_id_t, double, double, TRI_voc_tick_t>>
  getReplicationClients();

  // the ttl value is amount of seconds after which the client entry will
  // expire and may be garbage-collected
  void updateReplicationClient(TRI_server_id_t, double ttl);
  // the ttl value is amount of seconds after which the client entry will
  // expire and may be garbage-collected
  void updateReplicationClient(TRI_server_id_t, TRI_voc_tick_t, double ttl);
  // garbage collect replication clients that have an expire date later
  // than the specified timetamp
  void garbageCollectReplicationClients(double expireStamp);

  arangodb::DatabaseReplicationApplier* replicationApplier() const {
    return _replicationApplier.get();
  }
  void addReplicationApplier();

  arangodb::aql::QueryList* queryList() const { return _queries.get(); }
  arangodb::CursorRepository* cursorRepository() const {
    return _cursorRepository.get();
  }
  arangodb::CollectionKeysRepository* collectionKeys() const {
    return _collectionKeys.get();
  }

  bool isOwnAppsDirectory() const { return _isOwnAppsDirectory; }
  void setIsOwnAppsDirectory(bool value) { _isOwnAppsDirectory = value; }

  /// @brief signal the cleanup thread to wake up
  void signalCleanup();

  /// @brief increase the reference counter for a database.
  /// will return true if the refeence counter was increased, false otherwise
  /// in case false is returned, the database must not be used
  bool use();

  void forceUse();

  /// @brief decrease the reference counter for a database
  void release();

  /// @brief returns whether the database is dangling
  bool isDangling() const;

  /// @brief whether or not the vocbase has been marked as deleted
  bool isDropped() const;

  /// @brief marks a database as deleted
  bool markAsDropped();

  /// @brief returns whether the database is the system database
  bool isSystem() const { return name() == TRI_VOC_SYSTEM_DATABASE; }

  /// @brief closes a database and all collections
  void shutdown();

  /// @brief returns all known views
  std::vector<std::shared_ptr<arangodb::LogicalView>> views();

  /// @brief returns all known collections
  std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections(
    bool includeDeleted
  );

  void processCollections(std::function<void(arangodb::LogicalCollection*)> const& cb, bool includeDeleted);

  /// @brief returns names of all known collections
  std::vector<std::string> collectionNames();

  /// @brief creates a new view from parameter set
  std::shared_ptr<arangodb::LogicalView> createView(
    arangodb::velocypack::Slice parameters
  );

  /// @brief drops a view
  arangodb::Result dropView(TRI_voc_cid_t cid, bool allowDropSystem);

  /// @brief returns all known collections with their parameters
  /// and optionally indexes
  /// the result is sorted by type and name (vertices before edges)
  void inventory(arangodb::velocypack::Builder& result,
                 TRI_voc_tick_t, 
                 std::function<bool(arangodb::LogicalCollection const*)> const& nameFilter);

  /// @brief looks up a collection by identifier
  std::shared_ptr<arangodb::LogicalCollection> lookupCollection(
    TRI_voc_cid_t id
  ) const noexcept;

  /// @brief looks up a collection by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalCollection> lookupCollection(
    std::string const& nameOrId
  ) const noexcept;

  /// @brief looks up a collection by uuid
  std::shared_ptr<arangodb::LogicalCollection> lookupCollectionByUuid(
    std::string const& uuid
  ) const noexcept;

  /// @brief looks up a data-source by identifier
  std::shared_ptr<arangodb::LogicalDataSource> lookupDataSource(
    TRI_voc_cid_t id
  ) const noexcept;

  /// @brief looks up a data-source by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalDataSource> lookupDataSource(
    std::string const& nameOrId
  ) const noexcept;

  /// @brief looks up a view by identifier
  std::shared_ptr<arangodb::LogicalView> lookupView(
    TRI_voc_cid_t id
  ) const;

  /// @brief looks up a view by name or stringified cid or uuid
  std::shared_ptr<arangodb::LogicalView> lookupView(
    std::string const& nameOrId
  ) const;

  /// @brief renames a collection
  arangodb::Result renameCollection(
    TRI_voc_cid_t cid,
    std::string const& newName
  );

  /// @brief renames a view
  arangodb::Result renameView(
    TRI_voc_cid_t cid,
    std::string const& newName
  );

  /// @brief creates a new collection from parameter set
  /// collection id ("cid") is normally passed with a value of 0
  /// this means that the system will assign a new collection id automatically
  /// using a cid of > 0 is supported to import dumps from other servers etc.
  /// but the functionality is not advertised
  std::shared_ptr<arangodb::LogicalCollection> createCollection(
      arangodb::velocypack::Slice parameters);

  /// @brief drops a collection, no timeout if timeout is < 0.0, otherwise
  /// timeout is in seconds. Essentially, the timeout counts to acquire the
  /// write lock for using the collection.
  arangodb::Result dropCollection(
    TRI_voc_cid_t cid,
    bool allowDropSystem,
    double timeout
  );

  /// @brief callback for collection dropping
  static bool DropCollectionCallback(arangodb::LogicalCollection& collection);

  /// @brief unloads a collection
  int unloadCollection(arangodb::LogicalCollection* collection, bool force);

  /// @brief locks a collection for usage, loading or manifesting it
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself.
  int useCollection(arangodb::LogicalCollection* collection,
                    TRI_vocbase_col_status_e&);

  /// @brief locks a collection for usage by id
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
  /// when you are done with the collection.
  std::shared_ptr<arangodb::LogicalCollection> useCollection(TRI_voc_cid_t cid,
                                                             TRI_vocbase_col_status_e&);

  /// @brief locks a collection for usage by name
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
  /// when you are done with the collection.
   std::shared_ptr<arangodb::LogicalCollection> useCollection(std::string const& name,
                                                              TRI_vocbase_col_status_e&);

  /// @brief locks a collection for usage by uuid
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
  /// when you are done with the collection.
  std::shared_ptr<arangodb::LogicalCollection> useCollectionByUuid(std::string const& uuid,
                                                                   TRI_vocbase_col_status_e&);

  /// @brief releases a collection from usage
  void releaseCollection(arangodb::LogicalCollection* collection);

  /// @brief visit all DataSources registered with this vocbase
  /// @param visitor returns if visitation should continue
  /// @param lockWrite aquire write lock (if 'visitor' will modify vocbase)
  /// @return visitation compleated successfully
  typedef std::function<bool(arangodb::LogicalDataSource& dataSource)> dataSourceVisitor;
  bool visitDataSources(
    dataSourceVisitor const& visitor,
    bool lockWrite = false
  );

 private:

  /// @brief check some invariants on the various lists of collections
  void checkCollectionInvariants() const;

  std::shared_ptr<arangodb::LogicalCollection> useCollectionInternal(
       std::shared_ptr<arangodb::LogicalCollection>, TRI_vocbase_col_status_e& status);

  int loadCollection(arangodb::LogicalCollection* collection,
                     TRI_vocbase_col_status_e& status, bool setStatus = true);

  /// @brief adds a new collection
  /// caller must hold _dataSourceLock in write mode or set doLock
  void registerCollection(
    bool doLock,
    std::shared_ptr<arangodb::LogicalCollection> const& collection
  );

  /// @brief removes a collection from the global list of collections
  /// This function is called when a collection is dropped.
  bool unregisterCollection(arangodb::LogicalCollection* collection);

  /// @brief creates a new collection, worker function
  std::shared_ptr<arangodb::LogicalCollection> createCollectionWorker(
      arangodb::velocypack::Slice parameters);

  /// @brief drops a collection, worker function
  int dropCollectionWorker(arangodb::LogicalCollection* collection,
                           DropState& state, double timeout);

  /// @brief adds a new view
  /// caller must hold _dataSourceLock in write mode or set doLock
  void registerView(
    bool doLock,
    std::shared_ptr<arangodb::LogicalView> const& view
  );

  /// @brief removes a view from the global list of views
  /// This function is called when a view is dropped.
  bool unregisterView(arangodb::LogicalView const& view);
};

/// @brief extract the _rev attribute from a slice
TRI_voc_rid_t TRI_ExtractRevisionId(arangodb::velocypack::Slice const slice);

/// @brief extract the _rev attribute from a slice as a slice
arangodb::velocypack::Slice TRI_ExtractRevisionIdAsSlice(arangodb::velocypack::Slice const slice);

/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open
void TRI_SanitizeObject(arangodb::velocypack::Slice const slice,
                        arangodb::velocypack::Builder& builder);
void TRI_SanitizeObjectWithEdges(arangodb::velocypack::Slice const slice,
                                 arangodb::velocypack::Builder& builder);

#endif
