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
#include "VocBase/ViewImplementation.h"
#include "VocBase/voc-types.h"

#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

#include <functional>

class TRI_replication_applier_t;

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace aql {
class QueryList;
}
class CollectionNameResolver;
class CollectionKeysRepository;
class CursorRepository;
class LogicalCollection;
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
constexpr size_t TRI_JOURNAL_DEFAULT_SIZE = 1024 * 1024 * 32;  // 32 MB

////////////////////////////////////////////////////////////////////////////////
/// @brief minimal collection journal size (for testing, we allow very small
/// file sizes in maintainer mode)
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

constexpr size_t TRI_JOURNAL_MINIMAL_SIZE = 16 * 1024;  // 16 KB

#else

constexpr size_t TRI_JOURNAL_MINIMAL_SIZE = 1024 * 1024;  // 1 MB

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
  friend class arangodb::CollectionNameResolver;
  friend class arangodb::StorageEngine;

  /// @brief database state
  enum class State {
    NORMAL = 0,
    SHUTDOWN_COMPACTOR = 1,
    SHUTDOWN_CLEANUP = 2,
    FAILED_VERSION = 3
  };

  TRI_vocbase_t(TRI_vocbase_type_e type, TRI_voc_tick_t id,
                std::string const& name);
  ~TRI_vocbase_t();

 private:
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

  mutable arangodb::basics::ReadWriteLock _collectionsLock;  // collection iterator lock
  std::vector<arangodb::LogicalCollection*>
      _collections;  // pointers to ALL collections
  std::vector<arangodb::LogicalCollection*>
      _deadCollections;  // pointers to collections
                         // dropped that can be
                         // removed later

  std::unordered_map<std::string, arangodb::LogicalCollection*>
      _collectionsByName;  // collections by name
  std::unordered_map<TRI_voc_cid_t, arangodb::LogicalCollection*>
      _collectionsById;  // collections by id

  arangodb::basics::ReadWriteLock _viewsLock;  // views management lock
  std::unordered_map<std::string, std::shared_ptr<arangodb::LogicalView>>
      _viewsByName;  // views by name
  std::unordered_map<TRI_voc_cid_t, std::shared_ptr<arangodb::LogicalView>>
      _viewsById;  // views by id

  std::unique_ptr<arangodb::aql::QueryList> _queries;
  std::unique_ptr<arangodb::CursorRepository> _cursorRepository;
  std::unique_ptr<arangodb::CollectionKeysRepository> _collectionKeys;

  std::unique_ptr<TRI_replication_applier_t> _replicationApplier;

  arangodb::basics::ReadWriteLock _replicationClientsLock;
  std::unordered_map<TRI_server_id_t, std::pair<double, TRI_voc_tick_t>>
      _replicationClients;

 public:
  arangodb::basics::DeadlockDetector<arangodb::LogicalCollection>
      _deadlockDetector;
  arangodb::basics::ReadWriteLock _inventoryLock;  // object lock needed when
                                                   // replication is assessing
                                                   // the state of the vocbase

  // structures for user-defined volatile data
  void* _userStructures;

 public:
  /// @brief checks if a database name is allowed
  /// returns true if the name is allowed and false otherwise
  static bool IsAllowedName(bool allowSystem, std::string const& name);
  TRI_voc_tick_t id() const { return _id; }
  std::string const& name() const { return _name; }
  std::string path() const;
  TRI_vocbase_type_e type() const { return _type; }
  State state() const { return _state; }
  void setState(State state) { _state = state; }
  void updateReplicationClient(TRI_server_id_t, TRI_voc_tick_t);
  std::vector<std::tuple<TRI_server_id_t, double, TRI_voc_tick_t>>
  getReplicationClients();
  /// garbage collect replication clients
  void garbageCollectReplicationClients(double ttl);
  
  TRI_replication_applier_t* replicationApplier() const {
    return _replicationApplier.get();
  }
  void addReplicationApplier(TRI_replication_applier_t* applier);

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
  std::vector<arangodb::LogicalCollection*> collections(bool includeDeleted);
  
  void processCollections(std::function<void(arangodb::LogicalCollection*)> const& cb, bool includeDeleted);

  /// @brief returns names of all known collections
  std::vector<std::string> collectionNames();

  /// @brief get a collection name by a collection id
  /// the name is fetched under a lock to make this thread-safe.
  /// returns empty string if the collection does not exist.
  std::string collectionName(TRI_voc_cid_t id);

  /// @brief looks up a collection by name
  arangodb::LogicalCollection* lookupCollection(std::string const& name) const;
  /// @brief looks up a collection by identifier
  arangodb::LogicalCollection* lookupCollection(TRI_voc_cid_t id) const;

  /// @brief looks up a view by name
  std::shared_ptr<arangodb::LogicalView> lookupView(std::string const& name);
  /// @brief looks up a view by identifier
  std::shared_ptr<arangodb::LogicalView> lookupView(TRI_voc_cid_t id);

  /// @brief returns all known collections with their parameters
  /// and optionally indexes
  /// the result is sorted by type and name (vertices before edges)
  std::shared_ptr<arangodb::velocypack::Builder> inventory(
      TRI_voc_tick_t, bool (*)(arangodb::LogicalCollection*, void*), void*,
      bool, std::function<bool(arangodb::LogicalCollection*,
                               arangodb::LogicalCollection*)>);

  /// @brief renames a collection
  int renameCollection(arangodb::LogicalCollection* collection,
                       std::string const& newName, bool doOverride);

  /// @brief creates a new collection from parameter set
  /// collection id ("cid") is normally passed with a value of 0
  /// this means that the system will assign a new collection id automatically
  /// using a cid of > 0 is supported to import dumps from other servers etc.
  /// but the functionality is not advertised
  arangodb::LogicalCollection* createCollection(
      arangodb::velocypack::Slice parameters);

  /// @brief drops a collection, no timeout if timeout is < 0.0, otherwise
  /// timeout is in seconds. Essentially, the timeout counts to acquire the
  /// write lock for using the collection.
  int dropCollection(arangodb::LogicalCollection* collection,
                     bool allowDropSystem, double timeout);

  /// @brief callback for collection dropping
  static bool DropCollectionCallback(arangodb::LogicalCollection* collection);

  /// @brief unloads a collection
  int unloadCollection(arangodb::LogicalCollection* collection, bool force);

  /// @brief creates a new view from parameter set
  /// view id is normally passed with a value of 0
  /// this means that the system will assign a new id automatically
  /// using a cid of > 0 is supported to import dumps from other servers etc.
  /// but the functionality is not advertised
  std::shared_ptr<arangodb::LogicalView> createView(
      arangodb::velocypack::Slice parameters, TRI_voc_cid_t id);

  /// @brief drops a view
  int dropView(std::string const& name);
  int dropView(std::shared_ptr<arangodb::LogicalView> view);

  /// @brief locks a collection for usage, loading or manifesting it
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself.
  int useCollection(arangodb::LogicalCollection* collection,
                    TRI_vocbase_col_status_e&);

  /// @brief locks a collection for usage by id
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
  /// when you are done with the collection.
  arangodb::LogicalCollection* useCollection(TRI_voc_cid_t cid,
                                             TRI_vocbase_col_status_e&);

  /// @brief locks a collection for usage by name
  /// Note that this will READ lock the collection you have to release the
  /// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
  /// when you are done with the collection.
  arangodb::LogicalCollection* useCollection(std::string const& name,
                                             TRI_vocbase_col_status_e&);

  /// @brief releases a collection from usage
  void releaseCollection(arangodb::LogicalCollection* collection);

 private:
  /// @brief looks up a collection by name, without acquiring a lock
  arangodb::LogicalCollection* lookupCollectionNoLock(std::string const& name) const;

  int loadCollection(arangodb::LogicalCollection* collection,
                     TRI_vocbase_col_status_e& status, bool setStatus = true);

  /// @brief adds a new collection
  /// caller must hold _collectionsLock in write mode or set doLock
  void registerCollection(bool doLock, arangodb::LogicalCollection* collection);

  /// @brief removes a collection from the global list of collections
  /// This function is called when a collection is dropped.
  bool unregisterCollection(arangodb::LogicalCollection* collection);

  /// @brief creates a new collection, worker function
  arangodb::LogicalCollection* createCollectionWorker(
      arangodb::velocypack::Slice parameters);

  /// @brief drops a collection, worker function
  int dropCollectionWorker(arangodb::LogicalCollection* collection,
                           DropState& state, double timeout);

  /// @brief creates a new view, worker function
  std::shared_ptr<arangodb::LogicalView> createViewWorker(
      arangodb::velocypack::Slice parameters, TRI_voc_cid_t& id);

  /// @brief adds a new view
  /// caller must hold _viewsLock in write mode or set doLock
  void registerView(bool doLock, std::shared_ptr<arangodb::LogicalView> view);

  /// @brief removes a view from the global list of views
  /// This function is called when a view is dropped.
  bool unregisterView(std::shared_ptr<arangodb::LogicalView> view);
};

// scope guard for a database
// ensures that a database
class VocbaseGuard {
 public:
  VocbaseGuard() = delete;
  VocbaseGuard(VocbaseGuard const&) = delete;
  VocbaseGuard& operator=(VocbaseGuard const&) = delete;

  explicit VocbaseGuard(TRI_vocbase_t* vocbase) : _vocbase(vocbase) {
    if (!_vocbase->use()) {
      // database already dropped
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
  }
  ~VocbaseGuard() { _vocbase->release(); }
  TRI_vocbase_t* vocbase() const { return _vocbase; }

 private:
  TRI_vocbase_t* _vocbase;
};

/// @brief extract the _rev attribute from a slice
TRI_voc_rid_t TRI_ExtractRevisionId(VPackSlice const slice);

/// @brief extract the _rev attribute from a slice as a slice
VPackSlice TRI_ExtractRevisionIdAsSlice(VPackSlice const slice);

/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open
void TRI_SanitizeObject(VPackSlice const slice, VPackBuilder& builder);
void TRI_SanitizeObjectWithEdges(VPackSlice const slice, VPackBuilder& builder);

#endif
