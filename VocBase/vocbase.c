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
#include "VocBase/compactor.h"
#include "VocBase/document-collection.h"
#include "VocBase/query-cursor.h"
#include "VocBase/shadow-data.h"
#include "VocBase/simple-collection.h"
#include "VocBase/synchroniser.h"

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
  TRI_vocbase_col_t* col;

  // create a new proxy
  col = TRI_Allocate(sizeof(TRI_vocbase_col_t));
  col->_vocbase = vocbase;
  col->_type = type;
  TRI_CopyString(col->_name, name, sizeof(col->_name));
  col->_path = (path == NULL ? NULL : TRI_DuplicateString(path));
  col->_collection = NULL;
  col->_newBorn = 0;
  col->_loaded = 0;
  col->_corrupted = 0;
  col->_cid = cid;

  // check name
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, name, col, false);

  if (found != NULL) {
    TRI_Free(col);

    LOG_ERROR("duplicate entry for name '%s'", name);
    TRI_set_errno(TRI_ERROR_AVOCADO_DUPLICATE_NAME);

    return NULL;
  }

  // check collection identifier (unknown for new born collections)
  if (cid != 0) {
    found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsById, &cid, col, false);

    if (found != NULL) {
      TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, name);
      TRI_Free(col);

      LOG_ERROR("duplicate collection identifier '%lu' for name '%s'", (unsigned long) cid, name);
      TRI_set_errno(TRI_ERROR_AVOCADO_DUPLICATE_IDENTIFIER);

      return NULL;
    }
  }

  TRI_PushBackVectorPointer(&vocbase->_collections, col);
  return col;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans a directory and loads all collections
////////////////////////////////////////////////////////////////////////////////

static bool ScanPath (TRI_vocbase_t* vocbase, char const* path) {
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
            return false;
          }

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
  return true;
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
/// @brief checks if a collection is allowed
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
  int lockCheck;
  bool ok;

  if (! TRI_IsDirectory(path)) {
    LOG_ERROR("database path '%s' is not a directory", path);
    return NULL;
  }

  // check if the database is locked
  lockFile = TRI_Concatenate2File(path, "lock");
  lockCheck = TRI_VerifyLockFile(lockFile);

  if (lockCheck) {
    TRI_set_errno(TRI_ERROR_AVOCADO_DATABASE_LOCKED);

    LOG_FATAL("database is locked, please check the lock file '%s'", lockFile);
    TRI_FreeString(lockFile);
    return NULL;
  }

  if (TRI_ExistsFile(lockFile)) {
    TRI_UnlinkFile(lockFile);
  }

  lockCheck = TRI_CreateLockFile(lockFile);

  if (! lockCheck) {
    LOG_FATAL("cannot lock the database, please check the lock file '%s': %s", lockFile, TRI_last_error());
    TRI_FreeString(lockFile);

    return NULL;
  }

  // setup vocbase structure
  vocbase = TRI_Allocate(sizeof(TRI_vocbase_t));

  vocbase->_cursors = TRI_CreateShadowsQueryCursor();
  vocbase->_lockFile = lockFile;
  vocbase->_path = TRI_DuplicateString(path);

  TRI_InitReadWriteLock(&vocbase->_lock);

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

  // scan directory for collections
  ok = ScanPath(vocbase, vocbase->_path);

  if (! ok) {
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

void TRI_CloseVocBase (TRI_vocbase_t* vocbase) {
  vocbase->_active = 0;

  // TODO unload collections

  TRI_JoinThread(&vocbase->_synchroniser);
  TRI_JoinThread(&vocbase->_compactor);
  
  // cursors
  if (vocbase->_cursors) {
    TRI_FreeShadowStore(vocbase->_cursors);
  }
  
  TRI_DestroyLockFile(vocbase->_lockFile);
  TRI_FreeString(vocbase->_lockFile);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t const* TRI_LookupCollectionByNameVocBase (TRI_vocbase_t* vocbase, char const* name) {
  TRI_vocbase_col_t const* found;

  TRI_ReadLockReadWriteLock(&vocbase->_lock);
  found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);
  TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t const* TRI_LookupCollectionByIdVocBase (TRI_vocbase_t* vocbase, TRI_voc_cid_t id) {
  TRI_vocbase_col_t const* found;

  TRI_ReadLockReadWriteLock(&vocbase->_lock);
  found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id);
  TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds up a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t const* TRI_FindCollectionByNameVocBase (TRI_vocbase_t* vocbase, char const* name, bool bear) {
  TRI_vocbase_col_t const* found;

  TRI_ReadLockReadWriteLock(&vocbase->_lock);
  found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);
  TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

  if (found != NULL) {
    return found;
  }

  if (! bear) {
    return NULL;
  }

  return TRI_BearCollectionVocBase(vocbase, name);
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
/// @brief creates a new (document) collection
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t const* TRI_CreateCollectionVocBase (TRI_vocbase_t* vocbase, TRI_col_parameter_t* parameter) {
  TRI_doc_collection_t* collection;
  TRI_vocbase_col_t* vc;
  TRI_col_type_e type;
  char const* name;
  char wrong;
  void const* found;

  TRI_WriteLockReadWriteLock(&vocbase->_lock);

  // check that we have a new name
  name = parameter->_name;
  found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found != NULL) {
    TRI_set_errno(TRI_ERROR_AVOCADO_DUPLICATE_NAME);

    LOG_ERROR("collection named '%s' already exists", name);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  // check that the name does not contain any strange characters
  wrong = TRI_IsAllowedCollectionName(name);

  if (wrong != 0) {
    TRI_set_errno(TRI_ERROR_AVOCADO_ILLEGAL_NAME);

    LOG_ERROR("found illegal character in name: %c", wrong);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  // ok, construct the collection
  collection = NULL;
  type = parameter->_type;

  if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_sim_collection_t* sim;

    sim = TRI_CreateSimCollection(vocbase->_path, parameter);

    if (sim != NULL) {
      collection = &sim->base;
    }
  }
  else {
    TRI_set_errno(TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE);

    LOG_ERROR("unknown collection type: %d", parameter->_type);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  if (collection == NULL) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  vc = AddCollection(vocbase,
                     collection->base._type,
                     collection->base._name,
                     collection->base._cid,
                     collection->base._directory);

  if (vc == NULL) {
    if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
      TRI_CloseSimCollection((TRI_sim_collection_t*) collection);
      TRI_FreeSimCollection((TRI_sim_collection_t*) collection);
    }

    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  vc->_collection = collection;
  vc->_loaded = 1;

  TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
  return vc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads an existing (document) collection
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t const* TRI_LoadCollectionVocBase (TRI_vocbase_t* vocbase, char const* name) {
  TRI_col_type_e type;
  union { TRI_vocbase_col_t const* c; TRI_vocbase_col_t* v; } found;

  TRI_WriteLockReadWriteLock(&vocbase->_lock);

  // check that we have an existing name
  found.c = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found.c == NULL) {
    TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND);

    LOG_ERROR("unknown collection '%s'", name);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  // check if the collection is not already loaded
  if (found.c->_loaded || found.c->_corrupted) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return found.c;
  }

  // load the collection
  type = found.c->_type;

  if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_sim_collection_t* collection;

    collection = TRI_OpenSimCollection(found.c->_path);

    if (collection == NULL) {
      found.v->_corrupted = 1;
      LOG_ERROR("cannot load collection '%s'", name);
      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
      return NULL;
    }

    found.v->_collection = &collection->base;
    found.v->_loaded = 1;

    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return found.c;
  }
  else {
    TRI_set_errno(TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE);

    LOG_ERROR("unknown collection type %d for '%s'", (int) found.c->_type, name);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserves a new (document) collection or returns an existing one
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t const* TRI_BearCollectionVocBase (TRI_vocbase_t* vocbase, char const* name) {
  union { TRI_vocbase_col_t const* c; TRI_vocbase_col_t* v; } found;
  char wrong;

  TRI_WriteLockReadWriteLock(&vocbase->_lock);

  // check that we have an existing name
  found.c = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found.c != NULL) {
    return found.c;
  }

  // check that the name does not contain any strange characters
  wrong = TRI_IsAllowedCollectionName(name);

  if (wrong != 0) {
    TRI_set_errno(TRI_ERROR_AVOCADO_ILLEGAL_NAME);

    LOG_ERROR("found illegal character in name: %c", wrong);
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  // check if the collection is not already loaded
  found.v = AddCollection(vocbase, TRI_COL_TYPE_SIMPLE_DOCUMENT, name, 0, NULL);

  if (found.v == NULL) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return NULL;
  }

  found.v->_newBorn = 1;

  TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
  return found.c;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief manifests a new born (document) collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_ManifestCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t const* vc) {
  union { TRI_vocbase_col_t* v; TRI_vocbase_col_t const* c; } cnv;
  TRI_col_type_e type;
  TRI_doc_collection_t* collection;
  void* found;

  TRI_WriteLockReadWriteLock(&vocbase->_lock);

  cnv.c = vc;

  // maybe the collection is already manifested
  if (! vc->_newBorn) {
    if (vc->_corrupted) {
      TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_DATAFILE);

      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
      return false;
    }

    if (! vc->_loaded) {
      TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_DATAFILE);

      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
      return false;
    }

    if (vc->_collection == NULL) {
      TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_DATAFILE);

      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
      return false;
    }

    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return true;
  }

  // ok, construct the collection
  collection = NULL;
  type = vc->_type;

  if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_sim_collection_t* sim;
    TRI_col_parameter_t parameter;

    TRI_InitParameterCollection(&parameter, vc->_name, DEFAULT_MAXIMAL_SIZE);

    parameter._type = type;
    parameter._waitForSync = false;

    sim = TRI_CreateSimCollection(vocbase->_path, &parameter);

    if (sim != NULL) {
      collection = &sim->base;
    }
  }
  else {
    TRI_set_errno(TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE);

    cnv.v->_newBorn = 0;
    cnv.v->_corrupted = 1;

    LOG_ERROR("unknown collection type: %d", vc->_type);

    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return false;
  }

  if (collection == NULL) {
    TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_DATAFILE);

    cnv.v->_newBorn = 0;
    cnv.v->_corrupted = 1;

    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return false;
  }

  // fix collection entry
  cnv.v->_collection = collection;
  cnv.v->_newBorn = 0;
  cnv.v->_loaded = 1;
  cnv.v->_cid = collection->base._cid;

  // fix lookup tables
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsById, &vc->_cid, cnv.v, false);

  if (found != NULL) {
    LOG_ERROR("duplicate collection identifier '%lu' for name '%s'", (unsigned long) vc->_cid, vc->_name);
  }

  // release locks
  TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
  return true;
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

  // cannot rename a corrupted collection
  TRI_WriteLockReadWriteLock(&vocbase->_lock);

  if (col->_corrupted) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION);
  }

  // check if the new name is unused
  cnv.c = col;
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, newName, cnv.v, false);

  if (found != NULL) {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return TRI_set_errno(TRI_ERROR_AVOCADO_DUPLICATE_NAME);
  }

  // new born collection, no datafile/parameter file exists
  if (col->_newBorn) {
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, oldName);
    TRI_CopyString(col->_name, newName, sizeof(col->_name));
  }

  // collection is unloaded
  else if (! col->_loaded) {
    res = TRI_LoadParameterInfo(col->_path, &info);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, newName);
      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
      return res;
    }

    TRI_CopyString(info._name, newName, sizeof(info._name));

    res = TRI_SaveParameterInfo(col->_path, &info);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, newName);
      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
      return res;
    }

    TRI_CopyString(col->_name, newName, sizeof(col->_name));
  }

  // collection is loaded
  else if (col->_collection != NULL) {
    res = TRI_RenameCollection(&col->_collection->base, newName);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, newName);
      TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
      return res;
    }

    TRI_CopyString(col->_name, newName, sizeof(col->_name));
  }

  // upps,
  else {
    TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  
  // release locks
  TRI_WriteUnlockReadWriteLock(&vocbase->_lock);
  return TRI_ERROR_NO_ERROR;
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
