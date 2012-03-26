////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "vocbase.h"

#include <regex.h>
#include <sys/mman.h>

#include "BasicsC/files.h"
#include "BasicsC/hashes.h"
#include "BasicsC/locks.h"
#include "BasicsC/logging.h"
#include "BasicsC/random.h"
#include "BasicsC/strings.h"
#include "BasicsC/threads.h"
#include "VocBase/barrier.h"
#include "VocBase/compactor.h"
#include "VocBase/document-collection.h"
#include "VocBase/query-cursor.h"
#include "VocBase/shadow-data.h"
#include "VocBase/simple-collection.h"
#include "VocBase/synchroniser.h"
#include "VocBase/query-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief random server identifier (16 bit)
////////////////////////////////////////////////////////////////////////////////

static uint16_t ServerIdentifier = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief current tick identifier (48 bit)
////////////////////////////////////////////////////////////////////////////////

static uint64_t CurrentTick = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief tick lock
////////////////////////////////////////////////////////////////////////////////

static TRI_spin_t TickLock;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             COLLECTION DICTIONARY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyCid (TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_cid_t const* k = key;

  return *k;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementCid (TRI_associative_pointer_t* array, void const* element) {
  TRI_vocbase_col_t const* e = element;

  return e->_cid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection id and a collection
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyCid (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_voc_cid_t const* k = key;
  TRI_vocbase_col_t const* e = element;

  return *k == e->_cid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the collection name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyCollectionName (TRI_associative_pointer_t* array, void const* key) {
  char const* k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementCollectionName (TRI_associative_pointer_t* array, void const* element) {
  TRI_vocbase_col_t const* e = element;
  char const* name = e->_name;

  return TRI_FnvHashString(name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection id and a collection
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyCollectionName (TRI_associative_pointer_t* array, void const* key, void const* element) {
  char const* k = (char const*) key;
  TRI_vocbase_col_t const* e = element;

  return TRI_EqualString(k, e->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

static bool UnloadCollectionCallback (TRI_collection_t* col, void* data) {
  TRI_vocbase_col_t* collection;
  TRI_sim_collection_t* sim;
  int res;

  collection = data;

  TRI_WriteLockReadWriteLock(&collection->_lock);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADING) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return false;
  }

  if (collection->_collection == NULL) {
    collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return true;
  }

  if (collection->_collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    LOG_ERROR("cannot unload collection '%s' of type '%d'",
              collection->_name,
              (int) collection->_collection->base._type);

    collection->_status = TRI_VOC_COL_STATUS_LOADED;

    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return false;
  }

  sim = (TRI_sim_collection_t*) collection->_collection;

  res = TRI_CloseSimCollection(sim);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("failed to close collection '%s': %s",
              collection->_name,
              TRI_last_error());

    collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return true;
  }

  TRI_FreeSimCollection(sim);

  collection->_status = TRI_VOC_COL_STATUS_UNLOADED;
  collection->_collection = NULL;

  TRI_WriteUnlockReadWriteLock(&collection->_lock);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           VOCBASE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new collection
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* AddCollection (TRI_vocbase_t* vocbase,
                                         TRI_col_type_t type,
                                         char const* name,
                                         TRI_voc_cid_t cid,
                                         char const* path) {
  void const* found;
  TRI_vocbase_col_t* collection;

  // create a new proxy
  collection = TRI_Allocate(sizeof(TRI_vocbase_col_t));
  collection->_vocbase = vocbase;
  collection->_type = type;
  TRI_CopyString(collection->_name, name, sizeof(collection->_name));
  collection->_path = (path == NULL ? NULL : TRI_DuplicateString(path));
  collection->_collection = NULL;
  collection->_status = TRI_VOC_COL_STATUS_UNKNOWN;
  collection->_cid = cid;

  // check name
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, name, collection, false);

  if (found != NULL) {
    TRI_Free(collection);

    LOG_ERROR("duplicate entry for name '%s'", name);
    TRI_set_errno(TRI_ERROR_AVOCADO_DUPLICATE_NAME);

    return NULL;
  }

  // check collection identifier (unknown for new born collections)
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsById, &cid, collection, false);

  if (found != NULL) {
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, name);
    TRI_Free(collection);

    LOG_ERROR("duplicate collection identifier '%lu' for name '%s'", (unsigned long) cid, name);
    TRI_set_errno(TRI_ERROR_AVOCADO_DUPLICATE_IDENTIFIER);

    return NULL;
  }

  TRI_InitReadWriteLock(&collection->_lock);

  TRI_PushBackVectorPointer(&vocbase->_collections, collection);
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans a directory and loads all collections
////////////////////////////////////////////////////////////////////////////////

static int ScanPath (TRI_vocbase_t* vocbase, char const* path) {
  TRI_vector_string_t files;
  TRI_col_type_e type;
  regmatch_t matches[2];
  regex_t re;
  int res;
  size_t n;
  size_t i;

  files = TRI_FilesDirectory(path);
  n = files._length;

  regcomp(&re, "^collection-([0-9][0-9]*)$", REG_ICASE | REG_EXTENDED);

  for (i = 0;  i < n;  ++i) {
    char* name;
    char* file;

    name = files._buffer[i];

    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {
      LOG_DEBUG("ignoring file/directory '%s''", name);
      continue;
    }

    file = TRI_Concatenate2File(path, name);

    if (TRI_IsDirectory(file)) {
      TRI_col_info_t info;

      res = TRI_LoadParameterInfo(file, &info);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_DEBUG("ignoring directory '%s' without valid parameter file '%s'", file, TRI_COL_PARAMETER_FILE);
      }
      else {
        type = info._type;

        if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
          TRI_vocbase_col_t* c;

          c = AddCollection(vocbase, type, info._name, info._cid, file);

          if (c == NULL) {
            LOG_FATAL("failed to add simple document collection from '%s'", file);
            return TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION);
          }

          c->_status = TRI_VOC_COL_STATUS_UNLOADED;

          LOG_DEBUG("added simple document collection from '%s'", file);
        }
        else {
          LOG_DEBUG("skipping collection of unknown type %d", (int) type);
        }
      }
    }
    else {
      LOG_DEBUG("ignoring non-directory '%s'", file);
    }

    TRI_FreeString(file);
  }

  TRI_DestroyVectorString(&files);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief bears a new collection or returns an existing one by name
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* BearCollectionVocBase (TRI_vocbase_t* vocbase, char const* name) {
  union { void const* v; TRI_vocbase_col_t* c; } found;
  TRI_vocbase_col_t* collection;
  char wrong;

  TRI_WriteLockReadWriteLock(&vocbase->_lock);

  // .............................................................................
  // check that we have an existing name
  // .............................................................................

  found.v = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found.v != NULL) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return found.c;
  }

  // .............................................................................
  // create a new one
  // .............................................................................

  // check that the name does not contain any strange characters
  wrong = TRI_IsAllowedCollectionName(name);

  if (wrong != 0) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

    LOG_ERROR("found illegal character in name: %c", wrong);

    TRI_set_errno(TRI_ERROR_AVOCADO_ILLEGAL_NAME);
    return NULL;
  }

  // create a new collection
  collection = AddCollection(vocbase, TRI_COL_TYPE_SIMPLE_DOCUMENT, name, TRI_NewTickVocBase(), NULL);

  if (collection == NULL) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  collection->_status = TRI_VOC_COL_STATUS_NEW_BORN;

  TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief manifests a new born (document) collection
////////////////////////////////////////////////////////////////////////////////

static int ManifestCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  TRI_col_type_e type;

  TRI_WriteLockReadWriteLock(&collection->_lock);

  // cannot manifest a corrupted collection
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION);
  }

  // cannot manifest a deleted collection
  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND);
  }

  // loaded, unloaded, or unloading are manifested
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_ERROR_NO_ERROR;
  }

  if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_ERROR_NO_ERROR;
  }

  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_ERROR_NO_ERROR;
  }

  if (collection->_status != TRI_VOC_COL_STATUS_NEW_BORN) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // .............................................................................
  // manifest the collection
  // .............................................................................

  type = collection->_type;

  if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_sim_collection_t* sim;
    TRI_col_parameter_t parameter;

    TRI_InitParameterCollection(&parameter, collection->_name, DEFAULT_MAXIMAL_SIZE);

    parameter._type = type;
    parameter._waitForSync = false;

    sim = TRI_CreateSimCollection(vocbase->_path, &parameter, collection->_cid);

    if (sim == NULL) {
      collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

      TRI_WriteUnlockReadWriteLock(&collection->_lock);
      return TRI_errno();
    }

    collection->_status = TRI_VOC_COL_STATUS_LOADED;
    collection->_collection = &sim->base;

    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_ERROR_NO_ERROR;
  }
  else {
    collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

    LOG_ERROR("unknown collection type '%d' in collection '%s'", (int) type, collection->_name);

    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads an existing (document) collection
///
/// Note that this will READ lock the collection you have to release the
/// collection lock by yourself.
////////////////////////////////////////////////////////////////////////////////

static int LoadCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  TRI_col_type_e type;
  int res;

  // .............................................................................
  // read lock
  // .............................................................................

  // check if the collection is already loaded
  TRI_ReadLockReadWriteLock(&collection->_lock);

  if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {

    // DO NOT release the lock
    return TRI_ERROR_NO_ERROR;
  }

  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_ReadUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND);
  }

  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_ReadUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION);
  }

  // release the read lock and acquire a write lock, we have to do some work
  TRI_ReadUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // write lock
  // .............................................................................

  TRI_WriteLockReadWriteLock(&collection->_lock);

  // someone else loaded the collection, release the WRITE lock and try again
  if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return LoadCollectionVocBase(vocbase, collection);
  }

  // someone is trying to unload the collection, cancel this,
  // release the WRITE lock and try again
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    collection->_status = TRI_VOC_COL_STATUS_LOADED;
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return LoadCollectionVocBase(vocbase, collection);
  }

  // deleted, give up
  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND);
  }

  // corrupted, give up
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION);
  }

  // new born, manifest collection, release the WRITE lock and try again
  if (collection->_status == TRI_VOC_COL_STATUS_NEW_BORN) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);

    res = ManifestCollectionVocBase(vocbase, collection);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    return LoadCollectionVocBase(vocbase, collection);
  }

  // unloaded, load collection
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    type = collection->_type;

    if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
      TRI_sim_collection_t* sim;

      sim = TRI_OpenSimCollection(collection->_path);

      if (sim == NULL) {
        collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

        TRI_WriteUnlockReadWriteLock(&collection->_lock);
        return TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION);
      }

      collection->_collection = &sim->base;
      collection->_status = TRI_VOC_COL_STATUS_LOADED;

      // release the WRITE lock and try again
      TRI_WriteUnlockReadWriteLock(&collection->_lock);
      return LoadCollectionVocBase(vocbase, collection);
    }
    else {
      LOG_ERROR("unknown collection type %d for '%s'", (int) type, collection->_name);

      TRI_WriteUnlockReadWriteLock(&collection->_lock);
      return TRI_set_errno(TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE);
    }
  }

  LOG_ERROR("unknown collection status %d for '%s'", (int) collection->_status, collection->_name);

  TRI_WriteUnlockReadWriteLock(&collection->_lock);
  return TRI_set_errno(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief page size
////////////////////////////////////////////////////////////////////////////////

size_t PageSize;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a collection name is allowed
///
/// Returns 0 for success or the offending character.
////////////////////////////////////////////////////////////////////////////////

char TRI_IsAllowedCollectionName (char const* name) {
  bool ok;
  char const* ptr;

  for (ptr = name;  *ptr;  ++ptr) {
    if (name < ptr) {
      ok = (*ptr == '_') || ('0' <= *ptr && *ptr <= '9') || ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }
    else {
      ok = ('0' <= *ptr && *ptr <= '9') || ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (! ok) {
      return *ptr;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new tick
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_NewTickVocBase () {
  uint64_t tick = ServerIdentifier;

  TRI_LockSpin(&TickLock);

  tick |= (++CurrentTick) << 16;

  TRI_UnlockSpin(&TickLock);

  return tick;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTickVocBase (TRI_voc_tick_t tick) {
  TRI_voc_tick_t s = tick >> 16;

  TRI_LockSpin(&TickLock);

  if (CurrentTick < s) {
    CurrentTick = s;
  }

  TRI_UnlockSpin(&TickLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief msyncs a memory block between begin (incl) and end (excl)
////////////////////////////////////////////////////////////////////////////////

bool TRI_msync (int fd, char const* begin, char const* end) {
  intptr_t p = (intptr_t) begin;
  intptr_t q = (intptr_t) end;
  intptr_t g = (intptr_t) PageSize;

  char* b = (char*)( (p / g) * g );
  char* e = (char*)( ((q + g - 1) / g) * g );

  int res = msync(b, e - b, MS_SYNC);

#ifdef __APPLE__

  if (res == 0) {
    res = fcntl(fd, F_FULLFSYNC, 0);
  }

#endif

  if (res == 0) {
    return true;
  }
  else {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an exiting database, scans all collections
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_OpenVocBase (char const* path) {
  TRI_vocbase_t* vocbase;
  char* lockFile;
  int res;

  if (! TRI_IsDirectory(path)) {
    LOG_ERROR("database path '%s' is not a directory", path);

    TRI_set_errno(TRI_ERROR_AVOCADO_WRONG_VOCBASE_PATH);
    return NULL;
  }

  // .............................................................................
  // check that the database is not locked and lock it
  // .............................................................................

  lockFile = TRI_Concatenate2File(path, "lock");
  res = TRI_VerifyLockFile(lockFile);

  if (res == TRI_ERROR_NO_ERROR) {
    LOG_FATAL("database is locked, please check the lock file '%s'", lockFile);

    TRI_FreeString(lockFile);

    TRI_set_errno(TRI_ERROR_AVOCADO_DATABASE_LOCKED);
    return NULL;
  }

  if (TRI_ExistsFile(lockFile)) {
    TRI_UnlinkFile(lockFile);
  }

  res = TRI_CreateLockFile(lockFile);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_FATAL("cannot lock the database, please check the lock file '%s': %s", lockFile, TRI_last_error());

    TRI_FreeString(lockFile);
    return NULL;
  }

  // .............................................................................
  // setup vocbase structure
  // .............................................................................

  vocbase = TRI_Allocate(sizeof(TRI_vocbase_t));

  vocbase->_cursors = TRI_CreateShadowsQueryCursor();
    TRI_Free(vocbase);
    LOG_ERROR("out of memory when opening database");
    return NULL;
  }
  
  vocbase->_functions = TRI_InitialiseQueryFunctions();
  if (vocbase->_functions == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  vocbase->_lockFile = lockFile;
  vocbase->_path = TRI_DuplicateString(path);

  TRI_InitVectorPointer(&vocbase->_collections);

  TRI_InitAssociativePointer(&vocbase->_collectionsById,
                            HashKeyCid,
                            HashElementCid,
                            EqualKeyCid,
                            NULL);

  TRI_InitAssociativePointer(&vocbase->_collectionsByName,
                            HashKeyCollectionName,
                            HashElementCollectionName,
                            EqualKeyCollectionName,
                            NULL);

  TRI_InitReadWriteLock(&vocbase->_lock);

  // .............................................................................
  // scan directory for collections and start helper threads
  // .............................................................................

  res = ScanPath(vocbase, vocbase->_path);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativePointer(&vocbase->_collectionsByName);
    TRI_DestroyAssociativePointer(&vocbase->_collectionsById);
    TRI_DestroyVectorPointer(&vocbase->_collections);
    TRI_DestroyLockFile(vocbase->_lockFile);
    TRI_FreeString(vocbase->_lockFile);
    TRI_FreeShadowStore(vocbase->_cursors);
    TRI_Free(vocbase);
    TRI_DestroyReadWriteLock(&vocbase->_lock);

    return NULL;
  }
  
  // vocbase is now active
  vocbase->_active = 1;

  // start synchroniser thread
  TRI_InitThread(&vocbase->_synchroniser);
  TRI_StartThread(&vocbase->_synchroniser, TRI_SynchroniserVocBase, vocbase);

  // start compactor thread
  TRI_InitThread(&vocbase->_compactor);
  TRI_StartThread(&vocbase->_compactor, TRI_CompactorVocBase, vocbase);

  // we are done
  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a database and all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocBase (TRI_vocbase_t* vocbase) {
  size_t i;

  vocbase->_active = 0;

  // starts unloading of collections
  for (i = 0;  i < vocbase->_collections._length;  ++i) {
    TRI_vocbase_col_t* collection;

    collection = (TRI_vocbase_col_t*) vocbase->_collections._buffer[i];
    TRI_UnloadCollectionVocBase(vocbase, collection);
  }

  // stop synchroniser and compactor
  TRI_JoinThread(&vocbase->_synchroniser);
  TRI_JoinThread(&vocbase->_compactor);
  
  // clear the hashs and vectors
  TRI_DestroyAssociativePointer(&vocbase->_collectionsByName);
  TRI_DestroyAssociativePointer(&vocbase->_collectionsById);
  TRI_DestroyVectorPointer(&vocbase->_collections);

  // query functions
  if (vocbase->_functions) {
    TRI_FreeQueryFunctions(vocbase->_functions);

  // release lock on database
  TRI_DestroyLockFile(vocbase->_lockFile);
  TRI_FreeString(vocbase->_lockFile);

  // free the cursors
  TRI_FreeShadowStore(vocbase->_cursors);
  
  // destroy lock
  TRI_DestroyReadWriteLock(&vocbase->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_CollectionsVocBase (TRI_vocbase_t* vocbase) {
  TRI_vector_pointer_t result;
  TRI_vocbase_col_t* found;
  size_t i;

  TRI_InitVectorPointer(&result);

  TRI_ReadLockReadWriteLock(&vocbase->_lock);

  for (i = 0;  i < vocbase->_collectionsById._nrAlloc;  ++i) {
    found = vocbase->_collectionsById._table[i];

    if (found != NULL) {
      TRI_PushBackVectorPointer(&result, found);
    }
  }

  TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByNameVocBase (TRI_vocbase_t* vocbase, char const* name) {
  union { void const* v; TRI_vocbase_col_t* c; } found;

  TRI_ReadLockReadWriteLock(&vocbase->_lock);
  found.v = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);
  TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

  return found.c;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByIdVocBase (TRI_vocbase_t* vocbase, TRI_voc_cid_t id) {
  union { void const* v; TRI_vocbase_col_t* c; } found;

  TRI_ReadLockReadWriteLock(&vocbase->_lock);
  found.v = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id);
  TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

  return found.c;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_FindCollectionByNameVocBase (TRI_vocbase_t* vocbase, char const* name, bool bear) {
  union { void const* v; TRI_vocbase_col_t* c; } found;

  TRI_ReadLockReadWriteLock(&vocbase->_lock);
  found.v = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);
  TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

  if (found.v != NULL) {
    return found.c;
  }

  if (! bear) {
    TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND);
    return NULL;
  }

  return BearCollectionVocBase(vocbase, name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new (document) collection from parameter set
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_CreateCollectionVocBase (TRI_vocbase_t* vocbase, TRI_col_parameter_t* parameter) {
  TRI_doc_collection_t* doc;
  TRI_vocbase_col_t* collection;
  TRI_col_type_e type;
  char const* name;
  char wrong;
  void const* found;

  TRI_WriteLockReadWriteLock(&vocbase->_lock);

  // .............................................................................
  // check that we have a new name
  // .............................................................................

  name = parameter->_name;
  found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found != NULL) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

    LOG_ERROR("collection named '%s' already exists", name);

    TRI_set_errno(TRI_ERROR_AVOCADO_DUPLICATE_NAME);
    return NULL;
  }

  // check that the name does not contain any strange characters
  wrong = TRI_IsAllowedCollectionName(name);

  if (wrong != 0) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

    LOG_ERROR("found illegal character in name: %c", wrong);

    TRI_set_errno(TRI_ERROR_AVOCADO_ILLEGAL_NAME);
    return NULL;
  }

  // .............................................................................
  // ok, construct the collection
  // .............................................................................

  doc = NULL;
  type = parameter->_type;

  if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_sim_collection_t* sim;

    sim = TRI_CreateSimCollection(vocbase->_path, parameter, 0);

    if (sim == NULL) {
      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
      return NULL;
    }

    doc = &sim->base;
  }
  else {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

    LOG_ERROR("unknown collection type: %d", parameter->_type);

    TRI_set_errno(TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE);
    return NULL;
  }

  // add collection container
  collection = AddCollection(vocbase,
                             doc->base._type,
                             doc->base._name,
                             doc->base._cid,
                             doc->base._directory);

  if (collection == NULL) {
    if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
      TRI_CloseSimCollection((TRI_sim_collection_t*) doc);
      TRI_FreeSimCollection((TRI_sim_collection_t*) doc);
    }

    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  collection->_status = TRI_VOC_COL_STATUS_LOADED;
  collection->_collection = doc;

  TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnloadCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  TRI_WriteLockReadWriteLock(&collection->_lock);

  // cannot unload a corrupted collection
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION);
  }

  // a unloaded collection is unloaded
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_ERROR_NO_ERROR;
  }

  // a unloading collection is treated as unloaded
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_ERROR_NO_ERROR;
  }

  // a new born collection is treated as unloaded
  if (collection->_status == TRI_VOC_COL_STATUS_NEW_BORN) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_ERROR_NO_ERROR;
  }

  // a deleted collection is treated as unloaded
  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_ERROR_NO_ERROR;
  }

  // must be loaded
  if (collection->_status != TRI_VOC_COL_STATUS_LOADED) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // mark collection as unloading
  collection->_status = TRI_VOC_COL_STATUS_UNLOADING;

  // added callback for unload
  TRI_CreateBarrierCollection(&collection->_collection->_barrierList,
                              &collection->_collection->base,
                              UnloadCollectionCallback,
                              collection);

  // release locks
  TRI_WriteUnlockReadWriteLock(&collection->_lock);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* col, char const* newName) {
  TRI_col_info_t info;
  union { TRI_vocbase_col_t* v; TRI_vocbase_col_t const* c; } cnv;
  void* found;
  char wrong;
  char const* oldName;
  int res;

  // old name should be different
  oldName = col->_name;

  if (TRI_EqualString(oldName, newName)) {
    return TRI_ERROR_NO_ERROR;
  }

  // check name conventions
  wrong = TRI_IsAllowedCollectionName(newName);

  if (wrong != 0) {
    LOG_ERROR("found illegal character in name: %c", wrong);
    return TRI_set_errno(TRI_ERROR_AVOCADO_ILLEGAL_NAME);
  }

  // lock the vocbase because we are going to change the name
  TRI_WriteLockReadWriteLock(&vocbase->_lock);

  // lock collection because we are going to change the name
  TRI_WriteLockReadWriteLock(&col->_lock);

  // cannot rename a corrupted collection
  if (col->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WriteUnlockReadWriteLock(&col->_lock);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

    return TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION);
  }

  // cannot rename a deleted collection
  if (col->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_WriteUnlockReadWriteLock(&col->_lock);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

    return TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND);
  }

  // check if the new name is unused
  cnv.c = col;
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, newName, cnv.v, false);

  if (found != NULL) {
    TRI_WriteUnlockReadWriteLock(&col->_lock);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

    return TRI_set_errno(TRI_ERROR_AVOCADO_DUPLICATE_NAME);
  }

  // .............................................................................
  // new born collection, no datafile/parameter file exists
  // .............................................................................

  if (col->_status == TRI_VOC_COL_STATUS_NEW_BORN) {
    TRI_CopyString(col->_name, newName, sizeof(col->_name));
  }

  // .............................................................................
  // collection is unloaded
  // .............................................................................

  else if (col->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    res = TRI_LoadParameterInfo(col->_path, &info);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, newName);

      TRI_WriteUnlockReadWriteLock(&col->_lock);
      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

      return TRI_set_errno(res);
    }

    TRI_CopyString(info._name, newName, sizeof(info._name));

    res = TRI_SaveParameterInfo(col->_path, &info);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, newName);

      TRI_WriteUnlockReadWriteLock(&col->_lock);
      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

      return TRI_set_errno(res);
    }

    TRI_CopyString(col->_name, newName, sizeof(col->_name));
  }

  // .............................................................................
  // collection is loaded
  // .............................................................................

  else if (col->_status != TRI_VOC_COL_STATUS_LOADED || col->_status != TRI_VOC_COL_STATUS_UNLOADING) {
    res = TRI_RenameCollection(&col->_collection->base, newName);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, newName);

      TRI_WriteUnlockReadWriteLock(&col->_lock);
      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

      return TRI_set_errno(res);
    }

    TRI_CopyString(col->_name, newName, sizeof(col->_name));
  }

  // .............................................................................
  // upps, unknown status
  // .............................................................................

  else {
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, newName);

    TRI_WriteUnlockReadWriteLock(&col->_lock);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  
  // .............................................................................
  // remove old name and release locks
  // .............................................................................

  TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, oldName);

  TRI_WriteUnlockReadWriteLock(&col->_lock);
  TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage, loading or manifesting it
////////////////////////////////////////////////////////////////////////////////

int TRI_UseCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  return LoadCollectionVocBase(vocbase, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a (document) collection from usage
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  TRI_ReadUnlockReadWriteLock(&collection->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByNameVocBase (TRI_vocbase_t* vocbase, char const* name) {
  union { TRI_vocbase_col_t const* c; TRI_vocbase_col_t* v; } cnv;
  TRI_vocbase_col_t const* collection;
  int res;

  // .............................................................................
  // check that we have an existing name
  // .............................................................................

  TRI_WriteLockReadWriteLock(&vocbase->_lock);
  collection = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);
  TRI_WriteUnlockReadWriteLock(&vocbase->_lock);

  if (collection == NULL) {
    LOG_ERROR("unknown collection '%s'", name);

    TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND);
    return NULL;
  }

  // .............................................................................
  // try to load the collection
  // .............................................................................

  cnv.c = collection;
  res = LoadCollectionVocBase(vocbase, cnv.v);

  return res == TRI_ERROR_NO_ERROR ? cnv.v : NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the voc database components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseVocBase () {
  TRI_InitialiseHashes();
  TRI_InitialiseRandom();

  ServerIdentifier = TRI_UInt16Random();
  PageSize = getpagesize();

  TRI_InitSpin(&TickLock);

#ifdef TRI_READLINE_VERSION
  LOG_TRACE("%s", "$Revision: READLINE " TRI_READLINE_VERSION " $");
#endif

#ifdef TRI_V8_VERSION
  LOG_TRACE("%s", "$Revision: V8 " TRI_V8_VERSION " $");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the voc database components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownVocBase () {
  TRI_DestroySpin(&TickLock);

  TRI_ShutdownRandom();
  TRI_ShutdownHashes();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
