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
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "vocbase.h"

#include <regex.h>

#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/locks.h"
#include "Basics/logging.h"
#include "Basics/memory-map.h"
#include "Basics/random.h"
#include "Basics/tri-strings.h"
#include "Basics/threads.h"
#include "Basics/Exceptions.h"
#include "Utils/CursorRepository.h"
#include "Utils/transactions.h"
#include "VocBase/auth.h"
#include "VocBase/cleanup.h"
#include "VocBase/compactor.h"
#include "VocBase/Ditch.h"
#include "VocBase/document-collection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase-defaults.h"
#include "V8Server/v8-user-structures.h"
#include "Wal/LogfileManager.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep interval used when polling for a loading collection's status
////////////////////////////////////////////////////////////////////////////////

#define COLLECTION_STATUS_POLL_INTERVAL (1000 * 10)

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

static std::atomic<TRI_voc_tick_t> QueryId(1);

static std::atomic<bool> ThrowCollectionNotLoaded(false);

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief auxiliary struct for index iteration
////////////////////////////////////////////////////////////////////////////////

typedef struct index_json_helper_s {
  TRI_json_t*    _list;
  TRI_voc_tick_t _maxTick;
}
index_json_helper_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief states for TRI_DropCollectionVocBase()
////////////////////////////////////////////////////////////////////////////////

enum DropState {
  DROP_EXIT,       // drop done, nothing else to do
  DROP_AGAIN,      // drop not done, must try again
  DROP_PERFORM     // drop done, must perform actual cleanup routine
};

// -----------------------------------------------------------------------------
// --SECTION--                                             DICTIONARY FUNCTOIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyCid (TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_cid_t const* k = static_cast<TRI_voc_cid_t const*>(key);

  return *k;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementCid (TRI_associative_pointer_t* array, void const* element) {
  TRI_vocbase_col_t const* e = static_cast<TRI_vocbase_col_t const*>(element);

  return e->_cid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection id and a collection
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyCid (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_voc_cid_t const* k = static_cast<TRI_voc_cid_t const*>(key);
  TRI_vocbase_col_t const* e = static_cast<TRI_vocbase_col_t const*>(element);

  return *k == e->_cid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyCollectionName (TRI_associative_pointer_t* array, void const* key) {
  char const* k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementCollectionName (TRI_associative_pointer_t* array, void const* element) {
  TRI_vocbase_col_t const* e = static_cast<TRI_vocbase_col_t const*>(element);
  char const* name = e->_name;

  return TRI_FnvHashString(name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection name and a collection
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyCollectionName (TRI_associative_pointer_t* array, void const* key, void const* element) {
  char const* k = static_cast<char const*>(key);
  TRI_vocbase_col_t const* e = static_cast<TRI_vocbase_col_t const*>(element);

  return TRI_EqualString(k, e->_name);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           VOCBASE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief write a drop collection marker into the log
////////////////////////////////////////////////////////////////////////////////
  
static int WriteDropCollectionMarker (TRI_vocbase_t* vocbase,
                                      TRI_voc_cid_t collectionId) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    triagens::wal::DropCollectionMarker marker(vocbase->_id, collectionId);
    triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  }
  catch (triagens::basics::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_WARNING("could not save collection drop marker in log: %s", TRI_errno_string(res));
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a collection name from the global list of collections
///
/// This function is called when a collection is dropped.
/// note: the collection must be write-locked for this operation
////////////////////////////////////////////////////////////////////////////////

static bool UnregisterCollection (TRI_vocbase_t* vocbase,
                                  TRI_vocbase_col_t* collection) {
  TRI_ASSERT(collection != nullptr);

  WRITE_LOCKER(vocbase->_collectionsLock);

  // pre-condition
  TRI_ASSERT(vocbase->_collectionsByName._nrUsed == vocbase->_collectionsById._nrUsed);

  // only if we find the collection by its id, we can delete it by name
  if (TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsById, &collection->_cid) != nullptr) {
    // this is because someone else might have created a new collection with the same name,
    // but with a different id
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, collection->_name);
  }

  // post-condition
  TRI_ASSERT_EXPENSIVE(vocbase->_collectionsByName._nrUsed == vocbase->_collectionsById._nrUsed);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

static bool UnloadCollectionCallback (TRI_collection_t* col,
                                      void* data) {
  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(data);

  TRI_ASSERT(collection != nullptr);

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADING) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return false;
  }

  if (collection->_collection == nullptr) {
    collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return true;
  }

  auto ditches = collection->_collection->ditches();

  if (ditches->contains(triagens::arango::Ditch::TRI_DITCH_DOCUMENT) ||
      ditches->contains(triagens::arango::Ditch::TRI_DITCH_REPLICATION) ||
      ditches->contains(triagens::arango::Ditch::TRI_DITCH_COMPACTION)) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    // still some ditches left...
    // as the cleanup thread has already popped the unload ditch from the ditches list,
    // we need to insert a new one to really executed the unload
    TRI_UnloadCollectionVocBase(collection->_collection->_vocbase, collection, false);

    return false;
  }

  TRI_document_collection_t* document = collection->_collection;

  TRI_ASSERT(document != nullptr);

  int res = TRI_CloseDocumentCollection(document, true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("failed to close collection '%s': %s",
              collection->_name,
              TRI_last_error());

    collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return true;
  }

  TRI_FreeDocumentCollection(document);

  collection->_status = TRI_VOC_COL_STATUS_UNLOADED;
  collection->_collection = nullptr;

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

static bool DropCollectionCallback (TRI_collection_t* col,
                                    void* data) {
  TRI_vocbase_t* vocbase;
  regmatch_t matches[3];
  regex_t re;
  int res;

  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(data);

#ifdef _WIN32
  // .........................................................................
  // Just thank your lucky stars that there are only 4 backslashes
  // .........................................................................
  res = regcomp(&re, "^(.*)\\\\collection-([0-9][0-9]*)$", REG_ICASE | REG_EXTENDED);
#else
  res = regcomp(&re, "^(.*)/collection-([0-9][0-9]*)$", REG_ICASE | REG_EXTENDED);
#endif

  if (res != 0) {
    LOG_ERROR("unable to complile regular expression");

    return false;
  }


  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_DELETED) {
    LOG_ERROR("someone resurrected the collection '%s'", collection->_name);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    regfree(&re);

    return false;
  }

  // .............................................................................
  // unload collection
  // .............................................................................

  if (collection->_collection != nullptr) {
    TRI_document_collection_t* document = collection->_collection;

    res = TRI_CloseDocumentCollection(document, false);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("failed to close collection '%s': %s",
                collection->_name,
                TRI_last_error());

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      regfree(&re);

      return true;
    }

    TRI_FreeDocumentCollection(document);

    collection->_collection = nullptr;
  }

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  // .............................................................................
  // remove from list of collections
  // .............................................................................

  vocbase = collection->_vocbase;

  {
    WRITE_LOCKER(vocbase->_collectionsLock);

    auto it = std::find(vocbase->_collections.begin(), vocbase->_collections.end(), collection);

    if (it != vocbase->_collections.end()) {
      vocbase->_collections.erase(it);
    }

    // we need to clean up the pointers later so we insert it into this vector
    vocbase->_deadCollections.emplace_back(collection);
  }

  // .............................................................................
  // rename collection directory
  // .............................................................................

  if (*collection->_path != '\0') {
    int regExpResult;

    regExpResult = regexec(&re, collection->_path, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (regExpResult == 0) {
      char const* first = collection->_path + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

      char const* second = collection->_path + matches[2].rm_so;
      size_t secondLen = matches[2].rm_eo - matches[2].rm_so;

      char* tmp1;
      char* tmp2;
      char* tmp3;

      char* newFilename;

      tmp1 = TRI_DuplicateString2(first, firstLen);
      tmp2 = TRI_DuplicateString2(second, secondLen);
      tmp3 = TRI_Concatenate2String("deleted-", tmp2);

      TRI_FreeString(TRI_CORE_MEM_ZONE, tmp2);

      newFilename = TRI_Concatenate2File(tmp1, tmp3);

      TRI_FreeString(TRI_CORE_MEM_ZONE, tmp1);
      TRI_FreeString(TRI_CORE_MEM_ZONE, tmp3);

      // check if target directory already exists
      if (TRI_IsDirectory(newFilename)) {
        // no need to rename
        TRI_RemoveDirectory(newFilename);
      }
        
      // perform the rename
      res = TRI_RenameFile(collection->_path, newFilename);

      LOG_TRACE("renaming collection directory from '%s' to '%s'", collection->_path, newFilename);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_ERROR("cannot rename dropped collection '%s' from '%s' to '%s': %s",
                  collection->_name,
                  collection->_path,
                  newFilename,
                  TRI_errno_string(res));
      }
      else {
        LOG_DEBUG("wiping dropped collection '%s' from disk",
                  collection->_name);

        res = TRI_RemoveDirectory(newFilename);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_ERROR("cannot wipe dropped collection '%s' from disk: %s",
                    collection->_name,
                    TRI_errno_string(res));
        }
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, newFilename);
    }
    else {
      LOG_ERROR("cannot rename dropped collection '%s': unknown path '%s'",
                collection->_name,
                collection->_path);
    }
  }

  regfree(&re);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           VOCBASE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new collection
///
/// Caller must hold _collectionsLock in write mode
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* AddCollection (TRI_vocbase_t* vocbase,
                                         TRI_col_type_e type,
                                         char const* name,
                                         TRI_voc_cid_t cid,
                                         char const* path) {
  // create the init object
  TRI_vocbase_col_t init;

  init._vocbase         = vocbase;
  init._cid             = cid;
  init._planId          = 0;
  init._type            = static_cast<TRI_col_type_t>(type);
  init._internalVersion = 0;

  init._status          = TRI_VOC_COL_STATUS_CORRUPTED;
  init._collection      = nullptr;

  // default flags: everything is allowed
  init._canDrop         = true;
  init._canRename       = true;
  init._canUnload       = true;

  // check for special system collection names
  if (TRI_IsSystemNameCollection(name)) {
    // a few system collections have special behavior
    if (TRI_EqualString(name, TRI_COL_NAME_USERS) ||
        TRI_IsPrefixString(name, TRI_COL_NAME_STATISTICS)) {
      // these collections cannot be dropped or renamed
      init._canDrop   = false;
      init._canRename = false;
    }
  }

  TRI_CopyString(init._dbName, vocbase->_name, strlen(vocbase->_name));
  TRI_CopyString(init._name, name, sizeof(init._name) - 1);

  if (path == nullptr) {
    init._path[0] = '\0';
  }
  else {
    TRI_CopyString(init._path, path, TRI_COL_PATH_LENGTH);
  }

  // create a new proxy
  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_col_t), false));

  if (collection == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  memcpy(collection, &init, sizeof(TRI_vocbase_col_t));
  collection->_isLocal = true;

  // check name
  void const* found;
  int res = TRI_InsertKeyAssociativePointer2(&vocbase->_collectionsByName, name, collection, &found);

  if (found != nullptr) {
    LOG_ERROR("duplicate entry for collection name '%s'", name);
    LOG_ERROR("collection id %llu has same name as already added collection %llu",
              (unsigned long long) cid,
              (unsigned long long) ((TRI_vocbase_col_t*) found)->_cid);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);

    return nullptr;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // OOM. this might have happened AFTER insertion
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    TRI_set_errno(res);

    return nullptr;
  }

  // check collection identifier
  TRI_ASSERT(collection->_cid == cid);
  res = TRI_InsertKeyAssociativePointer2(&vocbase->_collectionsById, &cid, collection, &found);

  if (found != nullptr) {
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, name);

    LOG_ERROR("duplicate collection identifier %llu for name '%s'",
              (unsigned long long) collection->_cid, name);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);

    return nullptr;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // OOM. this might have happend AFTER insertion
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsById, &cid);
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    TRI_set_errno(res);

    return nullptr;
  }

  TRI_ASSERT_EXPENSIVE(vocbase->_collectionsByName._nrUsed == vocbase->_collectionsById._nrUsed);

  TRI_InitReadWriteLock(&collection->_lock);

  // this needs the write lock on _collectionsLock
  vocbase->_collections.emplace_back(collection);
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* CreateCollection (TRI_vocbase_t* vocbase,
                                            TRI_col_info_t* parameter,
                                            TRI_voc_cid_t& cid,
                                            bool writeMarker,
                                            TRI_json_t*& json) {
  TRI_ASSERT(json == nullptr);
  char const* name = parameter->_name;

  WRITE_LOCKER(vocbase->_collectionsLock);

  try {
    // reserve room for the new collection
    vocbase->_collections.reserve(vocbase->_collections.size() + 1);
    vocbase->_deadCollections.reserve(vocbase->_deadCollections.size() + 1);
  }
  catch (...) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // .............................................................................
  // check that we have a new name
  // .............................................................................

  void const* found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found != nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    return nullptr;
  }

  // .............................................................................
  // ok, construct the collection
  // .............................................................................

  TRI_document_collection_t* document = TRI_CreateDocumentCollection(vocbase, vocbase->_path, parameter, cid);

  if (document == nullptr) {
    return nullptr;
  }

  TRI_collection_t* col = document;

  // add collection container
  TRI_vocbase_col_t* collection = AddCollection(vocbase,
                                                col->_info._type,
                                                col->_info._name,
                                                col->_info._cid,
                                                col->_directory);

  if (collection == nullptr) {
    TRI_CloseDocumentCollection(document, false);
    TRI_FreeDocumentCollection(document);
    // TODO: does the collection directory need to be removed?
    return nullptr;
  }

  if (parameter->_planId > 0) {
    collection->_planId = parameter->_planId;
    col->_info._planId = parameter->_planId;
  }

  // cid might have been assigned
  cid = col->_info._cid;

  collection->_status = TRI_VOC_COL_STATUS_LOADED;
  collection->_collection = document;
  TRI_CopyString(collection->_path,
                 document->_directory,
                 sizeof(collection->_path) - 1);

  if (writeMarker) {
    json = TRI_CreateJsonCollectionInfo(&col->_info);
  }

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

static int RenameCollection (TRI_vocbase_t* vocbase,
                             TRI_vocbase_col_t* collection,
                             char const* oldName,
                             char const* newName) {
  // cannot rename a corrupted collection
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
  }

  // cannot rename a deleted collection
  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  {
    WRITE_LOCKER(vocbase->_collectionsLock);

    // check if the new name is unused
    void const* found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, newName);

    if (found != nullptr) {
      return TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    }

    // .............................................................................
    // collection is unloaded
    // .............................................................................

    else if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
      TRI_col_info_t info;
      int res = TRI_LoadCollectionInfo(collection->_path, &info, true);

      if (res != TRI_ERROR_NO_ERROR) {
        return TRI_set_errno(res);
      }

      TRI_CopyString(info._name, newName, sizeof(info._name) - 1);

      res = TRI_SaveCollectionInfo(collection->_path, &info, vocbase->_settings.forceSyncProperties);

      TRI_FreeCollectionInfoOptions(&info);

      if (res != TRI_ERROR_NO_ERROR) {
        return TRI_set_errno(res);
      }
      // fall-through intentional
    }

    // .............................................................................
    // collection is loaded
    // .............................................................................

    else if (collection->_status == TRI_VOC_COL_STATUS_LOADED ||
            collection->_status == TRI_VOC_COL_STATUS_UNLOADING ||
            collection->_status == TRI_VOC_COL_STATUS_LOADING) {

      int res = TRI_RenameCollection(collection->_collection, newName);

      if (res != TRI_ERROR_NO_ERROR) {
        return TRI_set_errno(res);
      }
      // fall-through intentional
    }

    // .............................................................................
    // unknown status
    // .............................................................................

    else {
      return TRI_set_errno(TRI_ERROR_INTERNAL);
    }

    // .............................................................................
    // rename and release locks
    // .............................................................................

    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, oldName);
    TRI_CopyString(collection->_name, newName, sizeof(collection->_name) - 1);

    // this shouldn't fail, as we removed an element above so adding one should be ok
    found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, newName, collection, false);
    TRI_ASSERT(found == nullptr);

    TRI_ASSERT_EXPENSIVE(vocbase->_collectionsByName._nrUsed == vocbase->_collectionsById._nrUsed);
  } // _colllectionsLock

  // to prevent caching returning now invalid old collection name in db's NamedPropertyAccessor,
  // i.e. db.<old-collection-name>
  collection->_internalVersion++;

  // invalidate all entries for the two collections
  triagens::aql::QueryCache::instance()->invalidate(vocbase, std::vector<char const*>{ oldName, newName });

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief this iterator is called on startup for journal and compactor file
/// of a collection
/// it will check the ticks of all markers and update the internal tick
/// counter accordingly. this is done so we'll not re-assign an already used
/// tick value
////////////////////////////////////////////////////////////////////////////////

static bool StartupTickIterator (TRI_df_marker_t const* marker,
                                 void* data,
                                 TRI_datafile_t* datafile) {
  auto tick = static_cast<TRI_voc_tick_t*>(data);

  if (marker->_tick > *tick) {
    *tick = marker->_tick;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans a directory and loads all collections
////////////////////////////////////////////////////////////////////////////////

static int ScanPath (TRI_vocbase_t* vocbase,
                     char const* path,
                     bool isUpgrade,
                     bool iterateMarkers) {
  TRI_vector_string_t files;
  regmatch_t matches[2];
  regex_t re;
  int res;
  size_t i, n;

  res = regcomp(&re, "^collection-([0-9][0-9]*)$", REG_EXTENDED);

  if (res != 0) {
    LOG_ERROR("unable to compile regular expression");

    return res;
  }

  files = TRI_FilesDirectory(path);
  n = files._length;

  if (iterateMarkers) {
    LOG_TRACE("scanning all collection markers in database '%s", vocbase->_name);
  }

  for (i = 0;  i < n;  ++i) {
    char* name = files._buffer[i];
    TRI_ASSERT(name != nullptr);

    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {
      // no match, ignore this file
      continue;
    }

    char* file = TRI_Concatenate2File(path, name);

    if (file == nullptr) {
      LOG_FATAL_AND_EXIT("out of memory");
    }

    if (TRI_IsDirectory(file)) {
      TRI_col_info_t info;

      if (! TRI_IsWritable(file)) {
        // the collection directory we found is not writable for the current user
        // this can cause serious trouble so we will abort the server start if we
        // encounter this situation
        LOG_ERROR("database subdirectory '%s' is not writable for current user", file);

        TRI_FreeString(TRI_CORE_MEM_ZONE, file);
        TRI_DestroyVectorString(&files);
        regfree(&re);

        return TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
      }

      // no need to lock as we are scanning
      res = TRI_LoadCollectionInfo(file, &info, true);

      if (res == TRI_ERROR_NO_ERROR) {
        TRI_UpdateTickServer(info._cid);
      }

      if (res != TRI_ERROR_NO_ERROR) {
        char* tmpfile = TRI_Concatenate2File(file, ".tmp");

        if (TRI_ExistsFile(tmpfile)) {
          LOG_TRACE("ignoring temporary directory '%s'", tmpfile);
          TRI_Free(TRI_CORE_MEM_ZONE, tmpfile);
          // temp file still exists. this means the collection was not created fully
          // and needs to be ignored
          TRI_FreeString(TRI_CORE_MEM_ZONE, file);
          TRI_FreeCollectionInfoOptions(&info);

          continue; // ignore this directory
        }
        
        TRI_Free(TRI_CORE_MEM_ZONE, tmpfile);

        LOG_ERROR("cannot read collection info file in directory '%s': %s", file, TRI_errno_string(res));
        TRI_FreeString(TRI_CORE_MEM_ZONE, file);
        TRI_DestroyVectorString(&files);
        TRI_FreeCollectionInfoOptions(&info);
        regfree(&re);
        return TRI_set_errno(res);
      }
      else if (info._deleted) {
        // we found a collection that is marked as deleted.
        // deleted collections should be removed on startup. this is the default
        LOG_DEBUG("collection '%s' was deleted, wiping it", name);

        res = TRI_RemoveDirectory(file);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_WARNING("cannot wipe deleted collection: %s", TRI_last_error());
        }
      }
      else {
        // we found a collection that is still active
        TRI_col_type_e type = info._type;
        TRI_vocbase_col_t* c;

        if (info._version < TRI_COL_VERSION) {
          // collection is too "old"

          if (! isUpgrade) {
            LOG_ERROR("collection '%s' has a too old version. Please start the server with the --upgrade option.",
                      info._name);

            TRI_FreeString(TRI_CORE_MEM_ZONE, file);
            TRI_DestroyVectorString(&files);
            TRI_FreeCollectionInfoOptions(&info);
            regfree(&re);

            return TRI_set_errno(res);
          }
          else {
            if (info._version < TRI_COL_VERSION_13) {
              LOG_ERROR("collection '%s' is too old to be upgraded with this ArangoDB version.", info._name);
              res = TRI_ERROR_ARANGO_ILLEGAL_STATE;
            }
            else {
              LOG_INFO("upgrading collection '%s'", info._name);
              res = TRI_ERROR_NO_ERROR;
            }

            if (res == TRI_ERROR_NO_ERROR && info._version < TRI_COL_VERSION_20) {
              res = TRI_UpgradeCollection20(vocbase, file, &info);
            }

            if (res != TRI_ERROR_NO_ERROR) {
              LOG_ERROR("upgrading collection '%s' failed.", info._name);

              TRI_FreeString(TRI_CORE_MEM_ZONE, file);
              TRI_DestroyVectorString(&files);
              TRI_FreeCollectionInfoOptions(&info);
              regfree(&re);

              return TRI_set_errno(res);
            }
          }
        }

        c = AddCollection(vocbase, type, info._name, info._cid, file);

        if (c == nullptr) {
          LOG_ERROR("failed to add document collection from '%s'", file);

          TRI_FreeString(TRI_CORE_MEM_ZONE, file);
          TRI_DestroyVectorString(&files);
          TRI_FreeCollectionInfoOptions(&info);
          regfree(&re);

          return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
        }

        c->_planId = info._planId;
        c->_status = TRI_VOC_COL_STATUS_UNLOADED;

        if (iterateMarkers) {
          // iterating markers may be time-consuming. we'll only do it if
          // we have to
          TRI_voc_tick_t tick;
          TRI_IterateTicksCollection(file, StartupTickIterator, &tick);

          TRI_UpdateTickServer(tick);
        }

        LOG_DEBUG("added document collection from '%s'", file);
      }
      TRI_FreeCollectionInfoOptions(&info);
    }
    else {
      LOG_DEBUG("ignoring non-directory '%s'", file);
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
  }

  regfree(&re);

  TRI_DestroyVectorString(&files);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads an existing (document) collection
///
/// Note that this will READ lock the collection. You have to release the
/// collection lock by yourself.
////////////////////////////////////////////////////////////////////////////////

static int LoadCollectionVocBase (TRI_vocbase_t* vocbase,
                                  TRI_vocbase_col_t* collection,
                                  TRI_vocbase_col_status_e& status,
                                  bool setStatus = true) {
  // .............................................................................
  // read lock
  // .............................................................................

  // check if the collection is already loaded
  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  // return original status to the caller
  if (setStatus) {
    status = collection->_status;
  }

  if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {

    // DO NOT release the lock
    return TRI_ERROR_NO_ERROR;
  }

  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
  }

  // release the read lock and acquire a write lock, we have to do some work
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  // .............................................................................
  // write lock
  // .............................................................................

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // someone else loaded the collection, release the WRITE lock and try again
  if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return LoadCollectionVocBase(vocbase, collection, status, false);
  }

  // someone is trying to unload the collection, cancel this,
  // release the WRITE lock and try again
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    // check if there is a deferred drop action going on for this collection
    if (collection->_collection->ditches()->contains(triagens::arango::Ditch::TRI_DITCH_COLLECTION_DROP)) {
      // drop call going on, we must abort
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      // someone requested the collection to be dropped, so it's not there anymore
      return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }

    // no drop action found, go on
    collection->_status = TRI_VOC_COL_STATUS_LOADED;

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return LoadCollectionVocBase(vocbase, collection, status, false);
  }

  // deleted, give up
  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  // corrupted, give up
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
  }

  // currently loading
  if (collection->_status == TRI_VOC_COL_STATUS_LOADING) {
    // loop until the status changes
    while (true) {
      TRI_vocbase_col_status_e status = collection->_status;

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      if (status != TRI_VOC_COL_STATUS_LOADING) {
        break;
      }

      // only throw this particular error if the server is configured to do so
      if (ThrowCollectionNotLoaded.load(std::memory_order_relaxed)) {
        return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED;      
      }

      usleep(COLLECTION_STATUS_POLL_INTERVAL);

      TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
    }

    return LoadCollectionVocBase(vocbase, collection, status, false);
  }

  // unloaded, load collection
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    // set the status to loading
    collection->_status = TRI_VOC_COL_STATUS_LOADING;

    // release the lock on the collection temporarily
    // this will allow other threads to check the collection's
    // status while it is loading (loading may take a long time because of
    // disk activity, index creation etc.)
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    TRI_document_collection_t* document = TRI_OpenDocumentCollection(vocbase, collection, IGNORE_DATAFILE_ERRORS);

    // lock again the adjust the status
    TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

    // no one else must have changed the status
    TRI_ASSERT(collection->_status == TRI_VOC_COL_STATUS_LOADING);

    if (document == nullptr) {
      collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
      return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
    }

    collection->_internalVersion = 0;
    collection->_collection = document;
    collection->_status = TRI_VOC_COL_STATUS_LOADED;
    TRI_CopyString(collection->_path, document->_directory, sizeof(collection->_path) - 1);

    // release the WRITE lock and try again
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return LoadCollectionVocBase(vocbase, collection, status, false);
  }

  LOG_ERROR("unknown collection status %d for '%s'", (int) collection->_status, collection->_name);

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
  return TRI_set_errno(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a (document) collection
////////////////////////////////////////////////////////////////////////////////

static int DropCollection (TRI_vocbase_t* vocbase,
                           TRI_vocbase_col_t* collection,
                           bool writeMarker,
                           DropState& state) {

  state = DROP_EXIT;

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  triagens::aql::QueryCache::instance()->invalidate(vocbase, collection->_name); 

  // .............................................................................
  // collection already deleted
  // .............................................................................

  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    // mark collection as deleted
    UnregisterCollection(vocbase, collection);

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // collection is unloaded
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_col_info_t info;

    int res = TRI_LoadCollectionInfo(collection->_path, &info, true);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      return TRI_set_errno(res);
    }

    if (! info._deleted) {
      info._deleted = true;

      // we don't need to fsync if we are in the recovery phase
      bool doSync = (vocbase->_settings.forceSyncProperties &&
                     ! triagens::wal::LogfileManager::instance()->isInRecovery());

      res = TRI_SaveCollectionInfo(collection->_path, &info, doSync);
      TRI_FreeCollectionInfoOptions(&info);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

        return TRI_set_errno(res);
      }
    }

    collection->_status = TRI_VOC_COL_STATUS_DELETED;
    UnregisterCollection(vocbase, collection);
    if (writeMarker) {
      WriteDropCollectionMarker(vocbase, collection->_cid);
    }

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    DropCollectionCallback(nullptr, collection);

    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // collection is loading
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_LOADING) {
    // loop until status changes
    
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    state = DROP_AGAIN;

    // try again later
    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // collection is loaded
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_LOADED || collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    collection->_collection->_info._deleted = true;
      
    bool doSync = (vocbase->_settings.forceSyncProperties &&
                   ! triagens::wal::LogfileManager::instance()->isInRecovery());

    int res = TRI_UpdateCollectionInfo(vocbase, collection->_collection, nullptr, doSync);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      return res;
    }

    collection->_status = TRI_VOC_COL_STATUS_DELETED;

    UnregisterCollection(vocbase, collection);
    if (writeMarker) {
      WriteDropCollectionMarker(vocbase, collection->_cid);
    }

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    state = DROP_PERFORM;
    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // unknown status
  // .............................................................................

  else {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    LOG_WARNING("internal error in TRI_DropCollectionVocBase");

    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter callback function for indexes
////////////////////////////////////////////////////////////////////////////////

static int FilterCollectionIndex (TRI_vocbase_col_t* collection,
                                  char const* filename,
                                  void* data) {
  index_json_helper_t* ij = (index_json_helper_t*) data;

  TRI_json_t* indexJson = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, nullptr);

  if (indexJson == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // compare index id with tick value
  TRI_json_t* id = TRI_LookupObjectJson(indexJson, "id");

  // index id is numeric
  if (TRI_IsNumberJson(id)) {
    uint64_t iid = (uint64_t) id->_value._number;

    if (iid > (uint64_t) ij->_maxTick) {
      // index too new
      TRI_FreeJson(TRI_CORE_MEM_ZONE, indexJson);
    }
    else {
      // convert "id" to string
      char* idString = TRI_StringUInt64(iid);
      TRI_InitStringJson(id, idString, strlen(idString));
      TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, ij->_list, indexJson);
    }
  }

  // index id is a string
  else if (TRI_IsStringJson(id)) {
    uint64_t iid = TRI_UInt64String2(id->_value._string.data, id->_value._string.length - 1);

    if (iid > (uint64_t) ij->_maxTick) {
      // index too new
      TRI_FreeJson(TRI_CORE_MEM_ZONE, indexJson);
    }
    else {
      TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, ij->_list, indexJson);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if collection _trx is present and build a set of entries
/// with it. this is done to ensure compatibility with old datafiles
////////////////////////////////////////////////////////////////////////////////

static bool ScanTrxCallback (TRI_doc_mptr_t const* mptr,
                             TRI_document_collection_t* document,
                             void* data) {
  TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(data);

  char const* key = TRI_EXTRACT_MARKER_KEY(mptr);  // PROTECTED by trx in caller

  if (vocbase->_oldTransactions == nullptr) {
    vocbase->_oldTransactions = new std::set<TRI_voc_tid_t>;
  }

  uint64_t tid = TRI_UInt64String(key);

  vocbase->_oldTransactions->insert(static_cast<TRI_voc_tid_t>(tid));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if collection _trx is present and build a set of entries
/// with it. this is done to ensure compatibility with old datafiles
////////////////////////////////////////////////////////////////////////////////

static int ScanTrxCollection (TRI_vocbase_t* vocbase) {
  TRI_vocbase_col_t* collection = TRI_LookupCollectionByNameVocBase(vocbase, TRI_COL_NAME_TRANSACTION);

  if (collection == nullptr) {
    // collection not found, no problem - seems to be a newer ArangoDB version
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_INTERNAL;

  {
    triagens::arango::SingleCollectionReadOnlyTransaction trx(new triagens::arango::StandaloneTransactionContext(), vocbase, collection->_cid);

    res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_DocumentIteratorDocumentCollection(&trx, trx.documentCollection(), vocbase, &ScanTrxCallback);

    trx.finish(res);
  }

  // don't need the collection anymore, so unload it
  TRI_UnloadCollectionVocBase(vocbase, collection, true);

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief free the memory associated with a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionVocBase (TRI_vocbase_col_t* collection) {
  TRI_DestroyReadWriteLock(&collection->_lock);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vocbase object, without threads and some other attributes
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_CreateInitialVocBase (TRI_server_t* server,
                                         TRI_vocbase_type_e type,
                                         char const* path,
                                         TRI_voc_tick_t id,
                                         char const* name,
                                         TRI_vocbase_defaults_t const* defaults) {
  try {
    std::unique_ptr<TRI_vocbase_t> vocbase(new TRI_vocbase_t(server, type, path, id, name, defaults));

    return vocbase.release();
  }
  catch (...) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing database, scans all collections
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_OpenVocBase (TRI_server_t* server,
                                char const* path,
                                TRI_voc_tick_t id,
                                char const* name,
                                TRI_vocbase_defaults_t const* defaults,
                                bool isUpgrade,
                                bool iterateMarkers) {
  TRI_ASSERT(name != nullptr);
  TRI_ASSERT(path != nullptr);
  TRI_ASSERT(defaults != nullptr);

  TRI_vocbase_t* vocbase = TRI_CreateInitialVocBase(server, TRI_VOCBASE_TYPE_NORMAL, path, id, name, defaults);

  if (vocbase == nullptr) {
    return nullptr;
  }

  TRI_InitCompactorVocBase(vocbase);

  // .............................................................................
  // scan directory for collections
  // .............................................................................

  // scan the database path for collections
  // this will create the list of collections and their datafiles, and will also
  // determine the last tick values used (if iterateMarkers is true)

  int res = ScanPath(vocbase, vocbase->_path, isUpgrade, iterateMarkers);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyCompactorVocBase(vocbase);
    delete vocbase;
    TRI_set_errno(res);

    return nullptr;
  }

  ScanTrxCollection(vocbase);

  // .............................................................................
  // vocbase is now active
  // .............................................................................

  vocbase->_state = (sig_atomic_t) TRI_VOCBASE_STATE_NORMAL;

  // .............................................................................
  // start helper threads
  // .............................................................................

  // start cleanup thread
  TRI_InitThread(&vocbase->_cleanup);
  TRI_StartThread(&vocbase->_cleanup, nullptr, "[cleanup]", TRI_CleanupVocBase, vocbase);
      
  vocbase->_replicationApplier = TRI_CreateReplicationApplier(server, vocbase);

  if (vocbase->_replicationApplier == nullptr) {
    // TODO
    LOG_FATAL_AND_EXIT("initialising replication applier for database '%s' failed", vocbase->_name);
  }


  // we are done
  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a database and all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocBase (TRI_vocbase_t* vocbase) {
  // stop replication
  if (vocbase->_replicationApplier != nullptr) {
    TRI_StopReplicationApplier(vocbase->_replicationApplier, false);
  }

  // mark all cursors as deleted so underlying collections can be freed soon
  if (vocbase->_cursorRepository != nullptr) {
    static_cast<triagens::arango::CursorRepository*>(vocbase->_cursorRepository)->garbageCollect(true);
  }

  std::vector<TRI_vocbase_col_t*> collections;

  {
    WRITE_LOCKER(vocbase->_collectionsLock);
    collections = vocbase->_collections;
  }

  // from here on, the vocbase is unusable, i.e. no collections can be created/loaded etc.

  // starts unloading of collections
  for (auto& collection : collections) {
    TRI_UnloadCollectionVocBase(vocbase, collection, true);
  }

  // this will signal the synchroniser and the compactor threads to do one last iteration
  vocbase->_state = (sig_atomic_t) TRI_VOCBASE_STATE_SHUTDOWN_COMPACTOR;
  
  TRI_LockCondition(&vocbase->_compactorCondition);
  TRI_SignalCondition(&vocbase->_compactorCondition);
  TRI_UnlockCondition(&vocbase->_compactorCondition);

  if (vocbase->_hasCompactor) {
    int res = TRI_JoinThread(&vocbase->_compactor);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to join compactor thread: %s", TRI_errno_string(res));
    }
  }

  // this will signal the cleanup thread to do one last iteration
  vocbase->_state = (sig_atomic_t) TRI_VOCBASE_STATE_SHUTDOWN_CLEANUP;

  TRI_LockCondition(&vocbase->_cleanupCondition);
  TRI_SignalCondition(&vocbase->_cleanupCondition);
  TRI_UnlockCondition(&vocbase->_cleanupCondition);

  int res = TRI_JoinThread(&vocbase->_cleanup);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("unable to join cleanup thread: %s", TRI_errno_string(res));
  }

  // free dead collections (already dropped but pointers still around)
  for (auto& collection : vocbase->_deadCollections) {
    TRI_FreeCollectionVocBase(collection);
  }

  // free collections
  for (auto& collection : vocbase->_collections) {
    TRI_FreeCollectionVocBase(collection);
  }

  TRI_DestroyCompactorVocBase(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the compactor thread
////////////////////////////////////////////////////////////////////////////////

void TRI_StartCompactorVocBase (TRI_vocbase_t* vocbase) {
  TRI_ASSERT(! vocbase->_hasCompactor);

  LOG_TRACE("starting compactor for database '%s'", vocbase->_name);
  // start compactor thread
  TRI_InitThread(&vocbase->_compactor);
  TRI_StartThread(&vocbase->_compactor, nullptr, "[compactor]", TRI_CompactorVocBase, vocbase);
  vocbase->_hasCompactor = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the compactor thread
////////////////////////////////////////////////////////////////////////////////

int TRI_StopCompactorVocBase (TRI_vocbase_t* vocbase) {
  if (vocbase->_hasCompactor) {
    vocbase->_hasCompactor = false;

    LOG_TRACE("stopping compactor for database '%s'", vocbase->_name);
    int res = TRI_JoinThread(&vocbase->_compactor);

    if (res != TRI_ERROR_NO_ERROR) {
      return TRI_ERROR_INTERNAL;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_vocbase_col_t*> TRI_CollectionsVocBase (TRI_vocbase_t* vocbase) {
  std::vector<TRI_vocbase_col_t*> result;

  READ_LOCKER(vocbase->_collectionsLock);

  for (size_t i = 0;  i < vocbase->_collectionsById._nrAlloc;  ++i) {
    TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(vocbase->_collectionsById._table[i]);

    if (found != nullptr) {
      result.emplace_back(found);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns names of all known (document) collections
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TRI_CollectionNamesVocBase (TRI_vocbase_t* vocbase) {
  std::vector<std::string> result;

  READ_LOCKER(vocbase->_collectionsLock);

  for (size_t i = 0;  i < vocbase->_collectionsById._nrAlloc;  ++i) {
    TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(vocbase->_collectionsById._table[i]);

    if (found != nullptr) {
      char const* name = found->_name;

      if (name != nullptr) {
        result.emplace_back(std::string(name));
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections with their parameters
/// and indexes, up to a specific tick value
/// while the collections are iterated over, there will be a global lock so
/// that there will be consistent view of collections & their properties
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_InventoryCollectionsVocBase (TRI_vocbase_t* vocbase,
                                             TRI_voc_tick_t maxTick,
                                             bool (*filter)(TRI_vocbase_col_t*, void*),
                                             void* data) {
  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json == nullptr) {
    return nullptr;
  }

  std::vector<TRI_vocbase_col_t*> collections;

  // cycle on write-lock
  WRITE_LOCKER_EVENTUAL(vocbase->_inventoryLock, 1000);

  // copy collection pointers into vector so we can work with the copy without
  // the global lock
  {
    READ_LOCKER(vocbase->_collectionsLock);
    collections = vocbase->_collections;
  }

  for (auto& collection : collections) {
    TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

    if (collection->_status == TRI_VOC_COL_STATUS_DELETED ||
        collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
      // we do not need to care about deleted or corrupted collections
      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
      continue;
    }

    if (collection->_cid > maxTick) {
      // collection is too new
      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
      continue;
    }

    // check if we want this collection
    if (filter != nullptr && ! filter(collection, data)) {
      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
      continue;
    }

    TRI_json_t* result = TRI_CreateObjectJson(TRI_CORE_MEM_ZONE, 2);

    if (result != nullptr) {
      TRI_json_t* collectionInfo = TRI_ReadJsonCollectionInfo(collection);

      if (collectionInfo != nullptr) {
        TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, result, "parameters", collectionInfo);

        TRI_json_t* indexesInfo = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

        if (indexesInfo != nullptr) {
          index_json_helper_t ij;
          ij._list    = indexesInfo;
          ij._maxTick = maxTick;

          TRI_IterateJsonIndexesCollectionInfo(collection, &FilterCollectionIndex, &ij);
          TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, result, "indexes", indexesInfo);
        }
      }

      TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, json, result);
    }

    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a translation of a collection status
////////////////////////////////////////////////////////////////////////////////

const char* TRI_GetStatusStringCollectionVocBase (TRI_vocbase_col_status_e status) {
  switch (status) {
    case TRI_VOC_COL_STATUS_UNLOADED:
      return "unloaded";
    case TRI_VOC_COL_STATUS_LOADED:
      return "loaded";
    case TRI_VOC_COL_STATUS_UNLOADING:
      return "unloading";
    case TRI_VOC_COL_STATUS_DELETED:
      return "deleted";
    case TRI_VOC_COL_STATUS_LOADING:
      return "loading";
    case TRI_VOC_COL_STATUS_CORRUPTED:
    case TRI_VOC_COL_STATUS_NEW_BORN:
    default:
      return "unkown";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a collection name by a collection id
///
/// The name is fetched under a lock to make this thread-safe. Returns NULL if
/// the collection does not exist. It is the caller's responsibility to free the
/// name returned
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetCollectionNameByIdVocBase (TRI_vocbase_t* vocbase,
                                        const TRI_voc_cid_t id) {
  READ_LOCKER(vocbase->_collectionsLock);

  TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));

  if (found == nullptr) {
    return nullptr;
  }

  return TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, found->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByNameVocBase (TRI_vocbase_t* vocbase,
                                                      char const* name) {
  char const c = *name;

  // if collection name is passed as a stringified id, we'll use the lookupbyid function
  // this is safe because collection names must not start with a digit
  if (c >= '0' && c <= '9') {
    return TRI_LookupCollectionByIdVocBase(vocbase, TRI_UInt64String(name));
  }

  // otherwise we'll look up the collection by name
  READ_LOCKER(vocbase->_collectionsLock);
  return static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByIdVocBase (TRI_vocbase_t* vocbase,
                                                    TRI_voc_cid_t id) {
  READ_LOCKER(vocbase->_collectionsLock);
  return static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a collection by name, optionally creates it
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_FindCollectionByNameOrCreateVocBase (TRI_vocbase_t* vocbase,
                                                            char const* name,
                                                            const TRI_col_type_t type) {
  if (name == nullptr) {
    return nullptr;
  }
  
  TRI_vocbase_col_t* found = nullptr;

  {
    READ_LOCKER(vocbase->_collectionsLock);

    if (name[0] >= '0' && name[0] <= '9') {
      // support lookup by id, too
      try {
        TRI_voc_cid_t id = triagens::basics::StringUtils::uint64(name, strlen(name));
        found = static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));
      }
      catch (...) {
        // no need to throw here... found will still be a nullptr
      }
    }
    else {
      found = static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
    }
  }

  if (found != nullptr) {
    return found;
  }
  else {
    // collection not found. now create it
    TRI_col_info_t parameter;

    TRI_InitCollectionInfo(vocbase,
                           &parameter,
                           name,
                           (TRI_col_type_e) type,
                           (TRI_voc_size_t) vocbase->_settings.defaultMaximalSize,
                           nullptr);

    TRI_vocbase_col_t* collection = TRI_CreateCollectionVocBase(vocbase, &parameter, 0, true);
    TRI_FreeCollectionInfoOptions(&parameter);

    return collection;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new (document) collection from parameter set
///
/// collection id (cid) is normally passed with a value of 0
/// this means that the system will assign a new collection id automatically
/// using a cid of > 0 is supported to import dumps from other servers etc.
/// but the functionality is not advertised
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_CreateCollectionVocBase (TRI_vocbase_t* vocbase,
                                                TRI_col_info_t* parameters,
                                                TRI_voc_cid_t cid,
                                                bool writeMarker) {
  TRI_ASSERT(parameters != nullptr);

  // check that the name does not contain any strange characters
  if (! TRI_IsAllowedNameCollection(parameters->_isSystem, parameters->_name)) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);

    return nullptr;
  }

  READ_LOCKER(vocbase->_inventoryLock);

  TRI_json_t* json = nullptr;
  auto collection = CreateCollection(vocbase, parameters, cid, writeMarker, json);

  if (! writeMarker) {
    TRI_ASSERT(json == nullptr);
    return collection;
  }

  if (json == nullptr) {
    // TODO: what to do here?
    return collection;
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    triagens::wal::CreateCollectionMarker marker(vocbase->_id, cid, triagens::basics::JsonHelper::toString(json));
    triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return collection;
  }
  catch (triagens::basics::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
  LOG_WARNING("could not save collection create marker in log: %s", TRI_errno_string(res));

  // TODO: what to do here?
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnloadCollectionVocBase (TRI_vocbase_t* vocbase,
                                 TRI_vocbase_col_t* collection,
                                 bool force) {
  if (! collection->_canUnload && ! force) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // cannot unload a corrupted collection
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
  }

  // an unloaded collection is unloaded
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_ERROR_NO_ERROR;
  }

  // an unloading collection is treated as unloaded
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_ERROR_NO_ERROR;
  }

  // a loading collection
  if (collection->_status == TRI_VOC_COL_STATUS_LOADING) {
    // loop until status changes
    while (1) {
      TRI_vocbase_col_status_e status = collection->_status;

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
      if (status != TRI_VOC_COL_STATUS_LOADING) {
        break;
      }
      usleep(COLLECTION_STATUS_POLL_INTERVAL);

      TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
    }
    // if we get here, the status has changed
    return TRI_UnloadCollectionVocBase(vocbase, collection, force);
  }

  // a deleted collection is treated as unloaded
  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_ERROR_NO_ERROR;
  }

  // must be loaded
  if (collection->_status != TRI_VOC_COL_STATUS_LOADED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // mark collection as unloading
  collection->_status = TRI_VOC_COL_STATUS_UNLOADING;

  // add callback for unload
  collection->_collection->ditches()->createUnloadCollectionDitch(collection->_collection, collection, UnloadCollectionCallback, __FILE__, __LINE__);

  // release locks
  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  // wake up the cleanup thread
  TRI_LockCondition(&vocbase->_cleanupCondition);
  TRI_SignalCondition(&vocbase->_cleanupCondition);
  TRI_UnlockCondition(&vocbase->_cleanupCondition);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_DropCollectionVocBase (TRI_vocbase_t* vocbase,
                               TRI_vocbase_col_t* collection,
                               bool writeMarker) {

  TRI_ASSERT(collection != nullptr);

  if (! collection->_canDrop && 
      ! triagens::wal::LogfileManager::instance()->isInRecovery()) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  while (true) {
    DropState state = DROP_EXIT;
    int res;
    {
      READ_LOCKER(vocbase->_inventoryLock);

      res = DropCollection(vocbase, collection, writeMarker, state);
    }

    if (state == DROP_PERFORM) {
      if (triagens::wal::LogfileManager::instance()->isInRecovery()) {
        DropCollectionCallback(nullptr, collection);
      }
      else {
        // add callback for dropping
        collection->_collection->ditches()->createDropCollectionDitch(collection->_collection, collection, DropCollectionCallback, __FILE__, __LINE__);

        // wake up the cleanup thread
        TRI_LockCondition(&vocbase->_cleanupCondition);
        TRI_SignalCondition(&vocbase->_cleanupCondition);
        TRI_UnlockCondition(&vocbase->_cleanupCondition);
      }
    }

    if (state == DROP_PERFORM || state == DROP_EXIT) {
      return res;
    }

    // try again in next iteration
    TRI_ASSERT(state == DROP_AGAIN);
    usleep(COLLECTION_STATUS_POLL_INTERVAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollectionVocBase (TRI_vocbase_t* vocbase,
                                 TRI_vocbase_col_t* collection,
                                 char const* newName,
                                 bool doOverride,
                                 bool writeMarker) {
  if (! collection->_canRename) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  // lock collection because we are going to copy its current name
  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  // old name should be different
  char* oldName = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, collection->_name);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (oldName == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // check if names are actually different
  if (TRI_EqualString(oldName, newName)) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

    return TRI_ERROR_NO_ERROR;
  }

  if (! doOverride) {
    bool isSystem;
    isSystem = TRI_IsSystemNameCollection(oldName);

    if (isSystem && ! TRI_IsSystemNameCollection(newName)) {
      // a system collection shall not be renamed to a non-system collection name
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }
    else if (! isSystem && TRI_IsSystemNameCollection(newName)) {
      // a non-system collection shall not be renamed to a system collection name
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }

    if (! TRI_IsAllowedNameCollection(isSystem, newName)) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }
  }

  READ_LOCKER(vocbase->_inventoryLock);

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  int res = RenameCollection(vocbase, collection, oldName, newName);

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
  
  TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
  
  if (res == TRI_ERROR_NO_ERROR && writeMarker) {
    // now log the operation
    try {
      triagens::wal::RenameCollectionMarker marker(vocbase->_id, collection->_cid, std::string(newName));
      triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
      }

      return TRI_ERROR_NO_ERROR;
    }
    catch (triagens::basics::Exception const& ex) {
      res = ex.code();
    }
    catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_WARNING("could not save collection rename marker in log: %s", TRI_errno_string(res));
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage, loading or manifesting it
////////////////////////////////////////////////////////////////////////////////

int TRI_UseCollectionVocBase (TRI_vocbase_t* vocbase,
                              TRI_vocbase_col_t* collection,
                              TRI_vocbase_col_status_e& status) {
  return LoadCollectionVocBase(vocbase, collection, status);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by id
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByIdVocBase (TRI_vocbase_t* vocbase,
                                                 TRI_voc_cid_t cid,
                                                 TRI_vocbase_col_status_e& status) {
  // .............................................................................
  // check that we have an existing name
  // .............................................................................

  TRI_vocbase_col_t const* collection = nullptr;
  {
    READ_LOCKER(vocbase->_collectionsLock);
    collection = static_cast<TRI_vocbase_col_t const*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &cid));
  }

  if (collection == nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return nullptr;
  }

  // .............................................................................
  // try to load the collection
  // .............................................................................

  int res = LoadCollectionVocBase(vocbase, const_cast<TRI_vocbase_col_t*>(collection), status);

  if (res == TRI_ERROR_NO_ERROR) {
    return const_cast<TRI_vocbase_col_t*>(collection);
  }

  TRI_set_errno(res);

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByNameVocBase (TRI_vocbase_t* vocbase,
                                                   char const* name,
                                                   TRI_vocbase_col_status_e& status) {
  // .............................................................................
  // check that we have an existing name
  // .............................................................................

  TRI_vocbase_col_t const* collection = nullptr;
  
  {
    READ_LOCKER(vocbase->_collectionsLock);
    collection = static_cast<TRI_vocbase_col_t const*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
  }

  if (collection == nullptr) {
    LOG_DEBUG("unknown collection '%s'", name);

    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return nullptr;
  }

  // .............................................................................
  // try to load the collection
  // .............................................................................

  int res = LoadCollectionVocBase(vocbase, const_cast<TRI_vocbase_col_t*>(collection), status);

  if (res == TRI_ERROR_NO_ERROR) {
    return const_cast<TRI_vocbase_col_t*>(collection);
  }

  TRI_set_errno(res);  
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a (document) collection from usage
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseCollectionVocBase (TRI_vocbase_t* vocbase,
                                   TRI_vocbase_col_t* collection) {
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the vocbase has been marked as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDeletedVocBase (TRI_vocbase_t* vocbase) {
  auto refCount = vocbase->_refCount.load();
  // if the stored value is odd, it means the database has been marked as deleted
  return (refCount % 2 == 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

bool TRI_UseVocBase (TRI_vocbase_t* vocbase) {
  // increase the reference counter by 2. 
  // this is because we use odd values to indicate that the database has been
  // marked as deleted
  auto oldValue = vocbase->_refCount.fetch_add(2, std::memory_order_release);
  // check if the deleted bit is set
  return (oldValue % 2 != 1); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseVocBase (TRI_vocbase_t* vocbase) {
  // decrease the reference counter by 2. 
  // this is because we use odd values to indicate that the database has been
  // marked as deleted
#ifdef TRI_ENABLE_MAINTAINER_MODE
  auto oldValue = vocbase->_refCount.fetch_sub(2, std::memory_order_release);
  TRI_ASSERT(oldValue >= 2);
#else
  vocbase->_refCount.fetch_sub(2, std::memory_order_release);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief marks a database as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropVocBase (TRI_vocbase_t* vocbase) {
  auto oldValue = vocbase->_refCount.fetch_or(1, std::memory_order_release);
  // if the previously stored value is odd, it means the database has already been marked as deleted
  return (oldValue % 2 == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database can be removed
////////////////////////////////////////////////////////////////////////////////

bool TRI_CanRemoveVocBase (TRI_vocbase_t* vocbase) {
  auto refCount = vocbase->_refCount.load();
  // we are intentionally comparing with exactly 1 here, because a 1 means
  // that noone else references the database but it has been marked as deleted
  return (refCount == 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database is the system database
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemVocBase (TRI_vocbase_t* vocbase) {
  return TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a database name is allowed
///
/// Returns true if the name is allowed and false otherwise
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedNameVocBase (bool allowSystem,
                               char const* name) {
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is allowed
  for (char const* ptr = name;  *ptr;  ++ptr) {
    bool ok;
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
      else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    }
    else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') || ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (! ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next query id
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_NextQueryIdVocBase (TRI_vocbase_t* vocbase) {
  return QueryId.fetch_add(1, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the "throw collection not loaded error"
////////////////////////////////////////////////////////////////////////////////

bool TRI_GetThrowCollectionNotLoadedVocBase (TRI_vocbase_t* vocbase) {
  return ThrowCollectionNotLoaded.load(std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the "throw collection not loaded error"
////////////////////////////////////////////////////////////////////////////////

void TRI_SetThrowCollectionNotLoadedVocBase (TRI_vocbase_t* vocbase, 
                                             bool value) {
  ThrowCollectionNotLoaded.store(value, std::memory_order_seq_cst);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     TRI_vocbase_t
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vocbase object
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t::TRI_vocbase_t (TRI_server_t* server,
                              TRI_vocbase_type_e type,
                              char const* path,
                              TRI_voc_tick_t id,
                              char const* name,
                              TRI_vocbase_defaults_t const* defaults) 
  : _id(id),
    _path(nullptr),
    _name(nullptr),
    _type(type),
    _refCount(0),
    _server(server),
    _userStructures(nullptr),
    _queries(nullptr),
    _cursorRepository(nullptr),
    _authInfoLoaded(false),
    _hasCompactor(false),
    _isOwnAppsDirectory(true),
    _oldTransactions(nullptr),
    _replicationApplier(nullptr) {

  _queries = new triagens::aql::QueryList(this);
  _cursorRepository = new triagens::arango::CursorRepository(this);
 
  _path = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, path);
  _name = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, name);
    
  // use the defaults provided
  TRI_ApplyVocBaseDefaults(this, defaults);
    
  // init collections
  _collections.reserve(32);
  _deadCollections.reserve(32);

  TRI_InitAssociativePointer(&_collectionsById,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCid,
                             HashElementCid,
                             EqualKeyCid,
                             nullptr);

  TRI_InitAssociativePointer(&_collectionsByName,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCollectionName,
                             HashElementCollectionName,
                             EqualKeyCollectionName,
                             nullptr);

  TRI_InitAuthInfo(this);

  TRI_CreateUserStructuresVocBase(this);

  TRI_InitCondition(&_compactorCondition);
  TRI_InitCondition(&_cleanupCondition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a vocbase object
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t::~TRI_vocbase_t () {
  if (_userStructures != nullptr) {
    TRI_FreeUserStructuresVocBase(this);
  }

  // free replication
  if (_replicationApplier != nullptr) {
    TRI_FreeReplicationApplier(_replicationApplier);
  }

  delete _oldTransactions;

  TRI_DestroyCondition(&_cleanupCondition);
  TRI_DestroyCondition(&_compactorCondition);

  TRI_DestroyAuthInfo(this);

  TRI_DestroyAssociativePointer(&_collectionsByName);
  TRI_DestroyAssociativePointer(&_collectionsById);

  delete static_cast<triagens::arango::CursorRepository*>(_cursorRepository);
  delete static_cast<triagens::aql::QueryList*>(_queries);
  
  // free name and path
  if (_path != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, _path);
  }
  if (_name != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, _name);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
