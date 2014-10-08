////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_VOCBASE_H
#define ARANGODB_VOC_BASE_VOCBASE_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

#include "Basics/associative.h"
#include "Basics/locks.h"
#include "Basics/threads.h"
#include "Basics/vector.h"
#include "Basics/voc-errors.h"
#include "VocBase/vocbase-defaults.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_document_collection_t;
struct TRI_col_info_s;
struct TRI_general_cursor_store_s;
struct TRI_json_t;
struct TRI_server_s;
struct TRI_vector_pointer_s;
struct TRI_vector_string_s;
struct TRI_vocbase_defaults_s;
struct TRI_replication_applier_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the collections structure
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_COLLECTIONS_VOCBASE(a) \
  TRI_ReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the collections structure
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(a) \
  TRI_ReadUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the collections structure
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(a) \
  TRI_WriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the collections structure
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(a) \
  TRI_WriteUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_READ_LOCK_STATUS_VOCBASE_COL(a) \
  TRI_TryReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_STATUS_VOCBASE_COL(a) \
  TRI_ReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_STATUS_VOCBASE_COL(a) \
  TRI_ReadUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_WRITE_LOCK_STATUS_VOCBASE_COL(a) \
  TRI_TryWriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_STATUS_VOCBASE_COL(a) \
  TRI_WriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the vocbase collection status
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(a) \
  TRI_WriteUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the vocbase collection status using spinning
////////////////////////////////////////////////////////////////////////////////

#define TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(a) \
  while (! TRI_TRY_WRITE_LOCK_STATUS_VOCBASE_COL(a)) { \
    usleep(1000); \
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _from attribute
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ATTRIBUTE_FROM  "_from"

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _to attribute
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ATTRIBUTE_TO    "_to"

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _key attribute
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ATTRIBUTE_KEY   "_key"

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _rev attribute
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ATTRIBUTE_REV   "_rev"

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the _id attribute
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ATTRIBUTE_ID    "_id"

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the system database
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_SYSTEM_DATABASE "_system"

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal path length
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_PATH_LENGTH     (512)

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal name length
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_NAME_LENGTH     (64)

////////////////////////////////////////////////////////////////////////////////
/// @brief default maximal collection journal size
////////////////////////////////////////////////////////////////////////////////

#define TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE (1024 * 1024 * 32)

////////////////////////////////////////////////////////////////////////////////
/// @brief minimal collection journal size (for testing, we allow very small
/// file sizes in maintainer mode)
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

#define TRI_JOURNAL_MINIMAL_SIZE (16 * 1024)

#else

#define TRI_JOURNAL_MINIMAL_SIZE (1024 * 1024)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief journal overhead
////////////////////////////////////////////////////////////////////////////////

#define TRI_JOURNAL_OVERHEAD (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t))

////////////////////////////////////////////////////////////////////////////////
/// @brief document handle separator as character
////////////////////////////////////////////////////////////////////////////////

#define TRI_DOCUMENT_HANDLE_SEPARATOR_CHR '/'

////////////////////////////////////////////////////////////////////////////////
/// @brief document handle separator as string
////////////////////////////////////////////////////////////////////////////////

#define TRI_DOCUMENT_HANDLE_SEPARATOR_STR "/"

////////////////////////////////////////////////////////////////////////////////
/// @brief index handle separator as character
////////////////////////////////////////////////////////////////////////////////

#define TRI_INDEX_HANDLE_SEPARATOR_CHR '/'

////////////////////////////////////////////////////////////////////////////////
/// @brief index handle separator as string
////////////////////////////////////////////////////////////////////////////////

#define TRI_INDEX_HANDLE_SEPARATOR_STR "/"

////////////////////////////////////////////////////////////////////////////////
/// @brief no limit
////////////////////////////////////////////////////////////////////////////////

#define TRI_QRY_NO_LIMIT ((TRI_voc_size_t) (4294967295U))

////////////////////////////////////////////////////////////////////////////////
/// @brief no skip
////////////////////////////////////////////////////////////////////////////////

#define TRI_QRY_NO_SKIP ((TRI_voc_ssize_t) 0)

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief database state
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_VOCBASE_STATE_INACTIVE           = 0,
  TRI_VOCBASE_STATE_NORMAL             = 1,
  TRI_VOCBASE_STATE_SHUTDOWN_COMPACTOR = 2,
  TRI_VOCBASE_STATE_SHUTDOWN_CLEANUP   = 3,
  TRI_VOCBASE_STATE_FAILED_VERSION     = 4
}
TRI_vocbase_state_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief database type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_VOCBASE_TYPE_NORMAL      = 0,
  TRI_VOCBASE_TYPE_COORDINATOR = 1
}
TRI_vocbase_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief database
///
/// For the lock handling, see the document "LOCKS.md".
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_vocbase_s {
  TRI_voc_tick_t             _id;                 // internal database id
  char*                      _path;               // path to the data directory
  char*                      _name;               // database name
  TRI_vocbase_type_e         _type;               // type (normal or coordinator)

  struct {
    TRI_spin_t               _lock;               // a lock protecting the usage information
    uint32_t                 _refCount;           // reference counter
    bool                     _isDeleted;          // flag if database is marked as deleted
  }                          _usage;

  struct TRI_server_s*       _server;
  TRI_vocbase_defaults_t     _settings;

  TRI_read_write_lock_t      _lock;               // collection iterator lock
  TRI_vector_pointer_t       _collections;        // pointers to ALL collections
  TRI_vector_pointer_t       _deadCollections;    // pointers to collections dropped that can be removed later

  TRI_associative_pointer_t  _collectionsByName;  // collections by name
  TRI_associative_pointer_t  _collectionsById;    // collections by id

  TRI_read_write_lock_t      _inventoryLock;      // object lock needed when replication is assessing the state of the vocbase

  // structures for user-defined volatile data
  void*                      _userStructures;

  TRI_associative_pointer_t  _authInfo;
  TRI_associative_pointer_t  _authCache;
  TRI_read_write_lock_t      _authInfoLock;
  bool                       _authInfoLoaded;     // flag indicating whether the authentication info was loaded successfully
  bool                       _hasCompactor;
  bool                       _isOwnAppsDirectory;

  std::set<TRI_voc_tid_t>*   _oldTransactions;

  struct TRI_replication_applier_t* _replicationApplier;

  // state of the database
  // 0 = inactive
  // 1 = normal operation/running
  // 2 = shutdown in progress/waiting for compactor/synchroniser thread to finish
  // 3 = shutdown in progress/waiting for cleanup thread to finish
  // 4 = version check failed

  sig_atomic_t               _state;

  TRI_thread_t               _compactor;
  TRI_thread_t               _cleanup;

  struct TRI_general_cursor_store_s* _cursors;
  TRI_associative_pointer_t* _functions;

  struct {
    TRI_read_write_lock_t _lock;
    TRI_vector_t          _data;
  }
  _compactionBlockers;

  TRI_condition_t            _compactorCondition;
  TRI_condition_t            _cleanupCondition;
}
TRI_vocbase_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief status of a collection
///
/// note: the NEW_BORN status is not used in ArangoDB 1.3 anymore, but is left
/// in this enum for compatibility with earlier versions
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_VOC_COL_STATUS_CORRUPTED = 0,
  TRI_VOC_COL_STATUS_NEW_BORN  = 1, // DEPRECATED, and shouldn't be used anymore
  TRI_VOC_COL_STATUS_UNLOADED  = 2,
  TRI_VOC_COL_STATUS_LOADED    = 3,
  TRI_VOC_COL_STATUS_UNLOADING = 4,
  TRI_VOC_COL_STATUS_DELETED   = 5,
  TRI_VOC_COL_STATUS_LOADING   = 6
}
TRI_vocbase_col_status_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection container
///
/// For the lock, handling see the document "LOCKS.md".
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_vocbase_col_s {
  TRI_vocbase_t*                    _vocbase;

  TRI_voc_cid_t                     _cid;        // local collecttion identifier
  TRI_voc_cid_t                     _planId;     // cluster-wide collecttion identifier
  TRI_col_type_t                    _type;       // collection type

  TRI_read_write_lock_t             _lock;       // lock protecting the status and name

  uint32_t                          _internalVersion; // is incremented when a collection is renamed
                                                 // this is used to prevent caching of collection objects
                                                 // with "wrong" names in the "db" object
  TRI_vocbase_col_status_e          _status;     // status of the collection
  struct TRI_document_collection_t*  _collection; // NULL or pointer to loaded collection
  char _name[TRI_COL_NAME_LENGTH + 1];           // name of the collection
  char _path[TRI_COL_PATH_LENGTH + 1];           // path to the collection files
  char _dbName[TRI_COL_NAME_LENGTH + 1];         // name of the database

  bool                              _isLocal;    // if true, the collection is local. if false,
                                                 // the collection is a remote (cluster) collection
  bool                              _canDrop;    // true if the collection can be dropped
  bool                              _canUnload;  // true if the collection can be unloaded
  bool                              _canRename;  // true if the collection can be renamed
}
TRI_vocbase_col_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief free the memory associated with a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionVocBase (TRI_vocbase_col_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free the memory associated with all collections in a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionsVocBase (struct TRI_vector_pointer_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vocbase object, without threads and some other attributes
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_CreateInitialVocBase (struct TRI_server_s*,
                                         TRI_vocbase_type_e,
                                         char const*,
                                         TRI_voc_tick_t,
                                         char const*,
                                         struct TRI_vocbase_defaults_s const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an initial, not fully constructed vocbase
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyInitialVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing database, loads all collections
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_OpenVocBase (struct TRI_server_s*,
                                char const*,
                                TRI_voc_tick_t,
                                char const*,
                                struct TRI_vocbase_defaults_s const*,
                                bool,
                                bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a database and all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the compactor thread
////////////////////////////////////////////////////////////////////////////////

void TRI_StartCompactorVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the compactor thread
////////////////////////////////////////////////////////////////////////////////

int TRI_StopCompactorVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known collections
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_CollectionsVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns names of all known collections
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_CollectionNamesVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections with their parameters
/// and optionally indexes
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_t* TRI_InventoryCollectionsVocBase (TRI_vocbase_t*,
                                                    TRI_voc_tick_t,
                                                    bool (*)(TRI_vocbase_col_t*, void*),
                                                    void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a translation of a collection status
////////////////////////////////////////////////////////////////////////////////

const char* TRI_GetStatusStringCollectionVocBase (TRI_vocbase_col_status_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a collection name by a collection id
///
/// the name is fetched under a lock to make this thread-safe. returns NULL if
/// the collection does not exist
/// it is the caller's responsibility to free the name returned
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetCollectionNameByIdVocBase (TRI_vocbase_t*,
                                        const TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByNameVocBase (TRI_vocbase_t*,
                                                      char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByIdVocBase (TRI_vocbase_t*,
                                                    TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a collection by name or creates it
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_FindCollectionByNameOrCreateVocBase (TRI_vocbase_t*,
                                                            char const*,
                                                            const TRI_col_type_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new (document) collection from parameter set
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_CreateCollectionVocBase (TRI_vocbase_t*,
                                                struct TRI_col_info_s*,
                                                TRI_voc_cid_t cid, 
                                                bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnloadCollectionVocBase (TRI_vocbase_t*,
                                 TRI_vocbase_col_t*,
                                 bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_DropCollectionVocBase (TRI_vocbase_t*,
                               TRI_vocbase_col_t*,
                               bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollectionVocBase (TRI_vocbase_t*,
                                 TRI_vocbase_col_t*,
                                 char const*,
                                 bool,
                                 bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage, loading or manifesting it
///
/// Note that this will READ lock the collection you have to release the
/// collection lock by yourself.
////////////////////////////////////////////////////////////////////////////////

int TRI_UseCollectionVocBase (TRI_vocbase_t*,
                              TRI_vocbase_col_t*,
                              TRI_vocbase_col_status_e&);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by id
///
/// Note that this will READ lock the collection you have to release the
/// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
/// when you are done with the collection.
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByIdVocBase (TRI_vocbase_t*,
                                                 TRI_voc_cid_t,
                                                 TRI_vocbase_col_status_e&);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by name
///
/// Note that this will READ lock the collection you have to release the
/// collection lock by yourself and call @ref TRI_ReleaseCollectionVocBase
/// when you are done with the collection.
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByNameVocBase (TRI_vocbase_t*,
                                                   char const*,
                                                   TRI_vocbase_col_status_e&);

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a (document) collection from usage
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseCollectionVocBase (TRI_vocbase_t*,
                                   TRI_vocbase_col_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

bool TRI_UseVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief marks a database as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether any references are held on a database
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsUsedVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database can be removed
////////////////////////////////////////////////////////////////////////////////

bool TRI_CanRemoveVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database is the system database
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemVocBase (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a database name is allowed
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedNameVocBase (bool,
                               char const*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
