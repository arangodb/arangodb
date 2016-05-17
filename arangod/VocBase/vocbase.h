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
#include "Basics/DeadlockDetector.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/StringUtils.h"
#include "Basics/threads.h"
#include "Basics/vector.h"
#include "Basics/voc-errors.h"
#include "VocBase/vocbase-defaults.h"

#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

struct TRI_document_collection_t;
class TRI_replication_applier_t;
struct TRI_server_t;
class TRI_vocbase_col_t;
struct TRI_vocbase_defaults_t;

namespace arangodb {
namespace velocypack {
class Builder;
}
namespace aql {
class QueryList;
}
struct VocbaseAuthCache;
class VocbaseAuthInfo;
class VocbaseCollectionInfo;
class CollectionKeysRepository;
class CursorRepository;
}

extern bool IGNORE_DATAFILE_ERRORS;

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_READ_LOCK_STATUS_VOCBASE_COL(a) a->_lock.tryReadLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_STATUS_VOCBASE_COL(a) a->_lock.readLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_STATUS_VOCBASE_COL(a) a->_lock.unlock()

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_WRITE_LOCK_STATUS_VOCBASE_COL(a) a->_lock.tryWriteLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(a) a->_lock.unlock()

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the vocbase collection status using spinning
////////////////////////////////////////////////////////////////////////////////

#define TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(a) \
  a->_lock.tryWriteLock(1000)

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _from attribute
////////////////////////////////////////////////////////////////////////////////

constexpr auto TRI_VOC_ATTRIBUTE_FROM = "_from";

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _to attribute
////////////////////////////////////////////////////////////////////////////////

constexpr auto TRI_VOC_ATTRIBUTE_TO = "_to";

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _key attribute
////////////////////////////////////////////////////////////////////////////////

constexpr auto TRI_VOC_ATTRIBUTE_KEY = "_key";

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _rev attribute
////////////////////////////////////////////////////////////////////////////////

constexpr auto TRI_VOC_ATTRIBUTE_REV = "_rev";

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _id attribute
////////////////////////////////////////////////////////////////////////////////

constexpr auto TRI_VOC_ATTRIBUTE_ID = "_id";

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the system database
////////////////////////////////////////////////////////////////////////////////

constexpr auto TRI_VOC_SYSTEM_DATABASE = "_system";

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal path length
////////////////////////////////////////////////////////////////////////////////

constexpr size_t TRI_COL_PATH_LENGTH = 512;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal name length
////////////////////////////////////////////////////////////////////////////////

constexpr size_t TRI_COL_NAME_LENGTH = 64;

////////////////////////////////////////////////////////////////////////////////
/// @brief default maximal collection journal size
////////////////////////////////////////////////////////////////////////////////

constexpr size_t TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE = 1024 * 1024 * 32; // 32 MB

////////////////////////////////////////////////////////////////////////////////
/// @brief minimal collection journal size (for testing, we allow very small
/// file sizes in maintainer mode)
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

constexpr size_t TRI_JOURNAL_MINIMAL_SIZE = 16 * 1024; // 16 KB

#else

constexpr size_t TRI_JOURNAL_MINIMAL_SIZE = 1024 * 1024; // 1 MB

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief document handle separator as character
////////////////////////////////////////////////////////////////////////////////

constexpr char TRI_DOCUMENT_HANDLE_SEPARATOR_CHR = '/';

////////////////////////////////////////////////////////////////////////////////
/// @brief document handle separator as string
////////////////////////////////////////////////////////////////////////////////

constexpr auto TRI_DOCUMENT_HANDLE_SEPARATOR_STR = "/";

////////////////////////////////////////////////////////////////////////////////
/// @brief index handle separator as character
////////////////////////////////////////////////////////////////////////////////

constexpr char TRI_INDEX_HANDLE_SEPARATOR_CHR = '/';

////////////////////////////////////////////////////////////////////////////////
/// @brief index handle separator as string
////////////////////////////////////////////////////////////////////////////////

constexpr auto TRI_INDEX_HANDLE_SEPARATOR_STR = "/";

////////////////////////////////////////////////////////////////////////////////
/// @brief collection enum
////////////////////////////////////////////////////////////////////////////////

enum TRI_col_type_e {
  TRI_COL_TYPE_UNKNOWN = 0,           // only used when initializing
  TRI_COL_TYPE_DOCUMENT = 2,
  TRI_COL_TYPE_EDGE = 3
};

////////////////////////////////////////////////////////////////////////////////
/// @brief database state
////////////////////////////////////////////////////////////////////////////////

enum TRI_vocbase_state_e {
  TRI_VOCBASE_STATE_INACTIVE = 0,
  TRI_VOCBASE_STATE_NORMAL = 1,
  TRI_VOCBASE_STATE_SHUTDOWN_COMPACTOR = 2,
  TRI_VOCBASE_STATE_SHUTDOWN_CLEANUP = 3,
  TRI_VOCBASE_STATE_FAILED_VERSION = 4
};

////////////////////////////////////////////////////////////////////////////////
/// @brief database type
////////////////////////////////////////////////////////////////////////////////

enum TRI_vocbase_type_e {
  TRI_VOCBASE_TYPE_NORMAL = 0,
  TRI_VOCBASE_TYPE_COORDINATOR = 1
};

////////////////////////////////////////////////////////////////////////////////
/// @brief database
///
/// For the lock handling, see the document "LOCKS.md".
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_t {
  TRI_vocbase_t(TRI_server_t*, TRI_vocbase_type_e, char const*, TRI_voc_tick_t,
                char const*, struct TRI_vocbase_defaults_t const*);

  ~TRI_vocbase_t();

  TRI_voc_tick_t _id;        // internal database id
  char* _path;               // path to the data directory
  char* _name;               // database name
  TRI_vocbase_type_e _type;  // type (normal or coordinator)

  std::atomic<uint64_t> _refCount;

  TRI_server_t* _server;
  TRI_vocbase_defaults_t _settings;

  arangodb::basics::DeadlockDetector<TRI_document_collection_t>
      _deadlockDetector;

  arangodb::basics::ReadWriteLock _collectionsLock;  // collection iterator lock
  std::vector<TRI_vocbase_col_t*> _collections;  // pointers to ALL collections
  std::vector<TRI_vocbase_col_t*> _deadCollections;  // pointers to collections
                                                     // dropped that can be
                                                     // removed later

  std::unordered_map<std::string, TRI_vocbase_col_t*> _collectionsByName;  // collections by name
  std::unordered_map<TRI_voc_cid_t, TRI_vocbase_col_t*> _collectionsById;    // collections by id

  arangodb::basics::ReadWriteLock _inventoryLock;  // object lock needed when
                                                   // replication is assessing
                                                   // the state of the vocbase

  // structures for user-defined volatile data
  void* _userStructures;
  arangodb::aql::QueryList* _queries;
  arangodb::CursorRepository* _cursorRepository;
  arangodb::CollectionKeysRepository* _collectionKeys;

  std::unordered_map<std::string, arangodb::VocbaseAuthInfo*> _authInfo;
  std::unordered_map<std::string, arangodb::VocbaseAuthCache*> _authCache;
  arangodb::basics::ReadWriteLock _authInfoLock;
  bool _authInfoLoaded;  // flag indicating whether the authentication info was
                         // loaded successfully
  bool _hasCompactor;
  bool _isOwnAppsDirectory;

  class TRI_replication_applier_t* _replicationApplier;

  arangodb::basics::ReadWriteLock _replicationClientsLock;
  std::unordered_map<TRI_server_id_t, std::pair<double, TRI_voc_tick_t>>
      _replicationClients;

  // state of the database
  // 0 = inactive
  // 1 = normal operation/running
  // 2 = shutdown in progress/waiting for compactor/synchronizer thread to
  // finish
  // 3 = shutdown in progress/waiting for cleanup thread to finish
  // 4 = version check failed

  sig_atomic_t _state;

  TRI_thread_t _compactor;
  TRI_thread_t _cleanup;

  struct {
    TRI_read_write_lock_t _lock;
    TRI_vector_t _data;
  } _compactionBlockers;

  TRI_condition_t _compactorCondition;
  TRI_condition_t _cleanupCondition;

 public:
  void updateReplicationClient(TRI_server_id_t, TRI_voc_tick_t);
  std::vector<std::tuple<TRI_server_id_t, double, TRI_voc_tick_t>>
  getReplicationClients();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief status of a collection
///
/// note: the NEW_BORN status is not used in ArangoDB 1.3 anymore, but is left
/// in this enum for compatibility with earlier versions
////////////////////////////////////////////////////////////////////////////////

enum TRI_vocbase_col_status_e {
  TRI_VOC_COL_STATUS_CORRUPTED = 0,
  TRI_VOC_COL_STATUS_NEW_BORN = 1,  // DEPRECATED, and shouldn't be used anymore
  TRI_VOC_COL_STATUS_UNLOADED = 2,
  TRI_VOC_COL_STATUS_LOADED = 3,
  TRI_VOC_COL_STATUS_UNLOADING = 4,
  TRI_VOC_COL_STATUS_DELETED = 5,
  TRI_VOC_COL_STATUS_LOADING = 6
};

////////////////////////////////////////////////////////////////////////////////
/// @brief collection container
////////////////////////////////////////////////////////////////////////////////

class TRI_vocbase_col_t {
 public:
  TRI_vocbase_col_t(TRI_vocbase_col_t const&) = delete;
  TRI_vocbase_col_t& operator=(TRI_vocbase_col_t const&) = delete;
  TRI_vocbase_col_t() = delete;

  TRI_vocbase_col_t(TRI_vocbase_t* vocbase, TRI_col_type_e type,
                    std::string const& name, TRI_voc_cid_t cid,
                    std::string const& path);
  ~TRI_vocbase_col_t();

  // Leftover from struct
 public:
  TRI_vocbase_t* vocbase() const { return _vocbase; }
  TRI_voc_cid_t cid() const { return _cid; }
  TRI_voc_cid_t planId() const { return _planId; }
  TRI_col_type_t type() const { return _type; }
  uint32_t internalVersion() const { return _internalVersion; }
  std::string const& path() const { return _path; }
  char const* pathc_str() const { return _path.c_str(); }
  std::string const& dbName() const { return _dbName; }
  std::string name() const { return _name; }
  char const* namec_str() const { return _name.c_str(); }
  bool isLocal() const { return _isLocal; }
  bool canDrop() const { return _canDrop; }
  bool canUnload() const { return _canUnload; }
  bool canRename() const { return _canRename; }

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Transform the information for this collection to velocypack
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(bool,
                                                              TRI_voc_tick_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Transform the information for this collection to velocypack
  ///        The builder has to be an opened Type::Object
  //////////////////////////////////////////////////////////////////////////////

  void toVelocyPack(arangodb::velocypack::Builder&, bool, TRI_voc_tick_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Transform the information for the indexes of this collection to
  /// velocypack
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPackIndexes(
      TRI_voc_tick_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Transform the information for this collection to velocypack
  ///        The builder has to be an opened Type::Array
  //////////////////////////////////////////////////////////////////////////////

  void toVelocyPackIndexes(arangodb::velocypack::Builder&, TRI_voc_tick_t);

 public:
  TRI_vocbase_t* _vocbase;

  TRI_voc_cid_t _cid;     // local collecttion identifier
  TRI_voc_cid_t _planId;  // cluster-wide collection identifier
  TRI_col_type_t _type;   // collection type

  arangodb::basics::ReadWriteLock _lock;  // lock protecting the status and name

  uint32_t _internalVersion;  // is incremented when a collection is renamed
  // this is used to prevent caching of collection objects
  // with "wrong" names in the "db" object
  TRI_vocbase_col_status_e _status;  // status of the collection
  struct TRI_document_collection_t*
      _collection;            // NULL or pointer to loaded collection
  std::string const _path;    // path to the collection files
  std::string const _dbName;  // name of the database
  std::string _name;          // name of the collection

  bool _isLocal;    // if true, the collection is local. if false,
                    // the collection is a remote (cluster) collection
  bool _canDrop;    // true if the collection can be dropped
  bool _canUnload;  // true if the collection can be unloaded
  bool _canRename;  // true if the collection can be renamed
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vocbase object, without threads and some other attributes
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_CreateInitialVocBase(TRI_server_t*, TRI_vocbase_type_e,
                                        char const*, TRI_voc_tick_t,
                                        char const*,
                                        struct TRI_vocbase_defaults_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing database, loads all collections
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_OpenVocBase(TRI_server_t*, char const*, TRI_voc_tick_t,
                               char const*,
                               struct TRI_vocbase_defaults_t const*, bool,
                               bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a database and all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the compactor thread
////////////////////////////////////////////////////////////////////////////////

void TRI_StartCompactorVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the compactor thread
////////////////////////////////////////////////////////////////////////////////

int TRI_StopCompactorVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known collections
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_vocbase_col_t*> TRI_CollectionsVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns names of all known collections
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TRI_CollectionNamesVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections with their parameters
/// and optionally indexes
/// The result is sorted by type and name (vertices before edges)
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<arangodb::velocypack::Builder> TRI_InventoryCollectionsVocBase(
    TRI_vocbase_t*, TRI_voc_tick_t, bool (*)(TRI_vocbase_col_t*, void*), void*,
    bool, std::function<bool(TRI_vocbase_col_t*, TRI_vocbase_col_t*)>);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a translation of a collection status
////////////////////////////////////////////////////////////////////////////////

char const* TRI_GetStatusStringCollectionVocBase(TRI_vocbase_col_status_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a collection name by a collection id
///
/// the name is fetched under a lock to make this thread-safe. returns NULL if
/// the collection does not exist
/// it is the caller's responsibility to free the name returned
////////////////////////////////////////////////////////////////////////////////

std::string TRI_GetCollectionNameByIdVocBase(TRI_vocbase_t*, const TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByNameVocBase(TRI_vocbase_t* vocbase,
                                                     std::string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByIdVocBase(TRI_vocbase_t* vocbase,
                                                   TRI_voc_cid_t cid);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new (document) collection from parameter set
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_CreateCollectionVocBase(TRI_vocbase_t*,
                                               arangodb::VocbaseCollectionInfo&,
                                               TRI_voc_cid_t cid, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnloadCollectionVocBase(TRI_vocbase_t*, TRI_vocbase_col_t*, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_DropCollectionVocBase(TRI_vocbase_t*, TRI_vocbase_col_t*, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollectionVocBase(TRI_vocbase_t*, TRI_vocbase_col_t*, char const*,
                                bool, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage, loading or manifesting it
///
/// Note that this will READ lock the collection you have to release the
/// collection lock by yourself.
////////////////////////////////////////////////////////////////////////////////

int TRI_UseCollectionVocBase(TRI_vocbase_t*, TRI_vocbase_col_t*,
                             TRI_vocbase_col_status_e&);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by id
///
/// Note that this will READ lock the collection you have to release the
/// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
/// when you are done with the collection.
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByIdVocBase(TRI_vocbase_t*, TRI_voc_cid_t,
                                                TRI_vocbase_col_status_e&);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by name
///
/// Note that this will READ lock the collection you have to release the
/// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
/// when you are done with the collection.
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByNameVocBase(TRI_vocbase_t*, char const*,
                                                  TRI_vocbase_col_status_e&);

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a (document) collection from usage
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseCollectionVocBase(TRI_vocbase_t*, TRI_vocbase_col_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the vocbase has been marked as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDeletedVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

bool TRI_UseVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief marks a database as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database can be removed
////////////////////////////////////////////////////////////////////////////////

bool TRI_CanRemoveVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database is the system database
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a database name is allowed
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedNameVocBase(bool, char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next query id
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_NextQueryIdVocBase(TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the "throw collection not loaded error"
////////////////////////////////////////////////////////////////////////////////

bool TRI_GetThrowCollectionNotLoadedVocBase();

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the "throw collection not loaded error"
////////////////////////////////////////////////////////////////////////////////

void TRI_SetThrowCollectionNotLoadedVocBase(bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _rev attribute from a slice
////////////////////////////////////////////////////////////////////////////////

TRI_voc_rid_t TRI_ExtractRevisionId(VPackSlice const slice);
  
////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _rev attribute from a slice as a slice
////////////////////////////////////////////////////////////////////////////////

VPackSlice TRI_ExtractRevisionIdAsSlice(VPackSlice const slice);

////////////////////////////////////////////////////////////////////////////////
/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open
////////////////////////////////////////////////////////////////////////////////

void TRI_SanitizeObject(VPackSlice const slice, VPackBuilder& builder);
void TRI_SanitizeObjectWithEdges(VPackSlice const slice, VPackBuilder& builder);
  
#endif
