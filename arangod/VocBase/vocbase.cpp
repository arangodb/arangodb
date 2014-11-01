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

#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/locks.h"
#include "Basics/logging.h"
#include "Basics/memory-map.h"
#include "Basics/random.h"
#include "Basics/tri-strings.h"
#include "Basics/threads.h"

#include "Utils/Exception.h"
#include "Utils/transactions.h"
#include "VocBase/auth.h"
#include "VocBase/barrier.h"
#include "VocBase/cleanup.h"
#include "VocBase/compactor.h"
#include "VocBase/document-collection.h"
#include "VocBase/general-cursor.h"
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
  catch (triagens::arango::Exception const& ex) {
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
  TRI_ASSERT(collection->_name != nullptr);

  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

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

  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

static bool UnloadCollectionCallback (TRI_collection_t* col,
                                      void* data) {
  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(data);

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

  if (TRI_ContainsBarrierList(&collection->_collection->_barrierList, TRI_BARRIER_ELEMENT) ||
      TRI_ContainsBarrierList(&collection->_collection->_barrierList, TRI_BARRIER_COLLECTION_REPLICATION) ||
      TRI_ContainsBarrierList(&collection->_collection->_barrierList, TRI_BARRIER_COLLECTION_COMPACTION)) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    // still some barriers left...
    // as the cleanup thread has already popped the unload barrier from the barrier list,
    // we need to insert a new one to really executed the unload
    TRI_UnloadCollectionVocBase(collection->_collection->_vocbase, collection, false);

    return false;
  }

  TRI_document_collection_t* document = collection->_collection;

  int res = TRI_CloseDocumentCollection(document);

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

    res = TRI_CloseDocumentCollection(document);

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

  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  for (size_t i = 0;  i < vocbase->_collections._length;  ++i) {
    if (vocbase->_collections._buffer[i] == collection) {
      TRI_RemoveVectorPointer(&vocbase->_collections, i);
      break;
    }
  }

  // we need to clean up the pointers later so we insert it into this vector
  TRI_PushBackVectorPointer(&vocbase->_deadCollections, collection);

  // we are now done with the vocbase structure
  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

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
/// Caller must hold TRI_WRITE_LOCK_COLLECTIONS_VOCBASE.
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

  // this needs TRI_WRITE_LOCK_COLLECTIONS_VOCBASE
  TRI_PushBackVectorPointer(&vocbase->_collections, collection);
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* CreateCollection (TRI_vocbase_t* vocbase,
                                            TRI_col_info_t* parameter,
                                            TRI_voc_cid_t cid,
                                            bool writeMarker) {
  char const* name = parameter->_name;

  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  // .............................................................................
  // check that we have a new name
  // .............................................................................

  void const* found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found != nullptr) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    return nullptr;
  }

  // .............................................................................
  // ok, construct the collection
  // .............................................................................

  TRI_document_collection_t* document = TRI_CreateDocumentCollection(vocbase, vocbase->_path, parameter, cid);

  if (document == nullptr) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

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
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_CloseDocumentCollection(document);
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

  TRI_json_t* json = nullptr;

  if (writeMarker) {
    json = TRI_CreateJsonCollectionInfo(&col->_info);
    if (json == nullptr) {
      // TODO: revert the operation
      return collection;
    }
  }

  // release the lock on the list of collections
  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  if (! writeMarker) {
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
  catch (triagens::arango::Exception const& ex) {
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
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

static int RenameCollection (TRI_vocbase_t* vocbase,
                             TRI_vocbase_col_t* collection,
                             char const* oldName,
                             char const* newName,
                             bool writeMarker) {
  TRI_col_info_t info;
  void const* found;

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // this must be done after the collection lock
  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  // cannot rename a corrupted collection
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
  }

  // cannot rename a deleted collection
  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  // check if the new name is unused
  found = (void*) TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, newName);

  if (found != nullptr) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
  }

  // .............................................................................
  // collection is unloaded
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    int res = TRI_LoadCollectionInfo(collection->_path, &info, true);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      return TRI_set_errno(res);
    }

    TRI_CopyString(info._name, newName, sizeof(info._name) - 1);

    res = TRI_SaveCollectionInfo(collection->_path, &info, vocbase->_settings.forceSyncProperties);

    TRI_FreeCollectionInfoOptions(&info);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      return TRI_set_errno(res);
    }
  }

  // .............................................................................
  // collection is loaded
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_LOADED ||
           collection->_status == TRI_VOC_COL_STATUS_UNLOADING ||
           collection->_status == TRI_VOC_COL_STATUS_LOADING) {

    int res = TRI_RenameCollection(collection->_collection, newName);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      return TRI_set_errno(res);
    }
  }

  // .............................................................................
  // upps, unknown status
  // .............................................................................

  else {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // .............................................................................
  // rename and release locks
  // .............................................................................

  TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, oldName);
  TRI_CopyString(collection->_name, newName, sizeof(collection->_name) - 1);

  // this shouldn't fail, as we removed an element above so adding one should be ok
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, newName, CONST_CAST(collection), false);
  TRI_ASSERT(found == nullptr);

  TRI_ASSERT_EXPENSIVE(vocbase->_collectionsByName._nrUsed == vocbase->_collectionsById._nrUsed);

  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  // to prevent caching
  collection->_internalVersion++;

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (! writeMarker) {
    return TRI_ERROR_NO_ERROR;
  }

  // now log the operation
  int res = TRI_ERROR_NO_ERROR;

  try {
    triagens::wal::RenameCollectionMarker marker(vocbase->_id, collection->_cid, std::string(newName));
    triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    return TRI_ERROR_NO_ERROR;
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_WARNING("could not save collection rename marker in log: %s", TRI_errno_string(res));
  }

  return res;
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
  TRI_FastUpdateTickServer(marker->_tick);

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
        TRI_FastUpdateTickServer(info._cid);
      }

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_DEBUG("ignoring directory '%s' without valid parameter file '%s'", file, TRI_VOC_PARAMETER_FILE);
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
          TRI_IterateTicksCollection(file, StartupTickIterator, nullptr);
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
/// Note that this will READ lock the collection you have to release the
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
    if (TRI_ContainsBarrierList(&collection->_collection->_barrierList, TRI_BARRIER_COLLECTION_DROP_CALLBACK)) {
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
    while (1) {
      TRI_vocbase_col_status_e status = collection->_status;

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      if (status != TRI_VOC_COL_STATUS_LOADING) {
        break;
      }
      usleep(COLLECTION_STATUS_POLL_INTERVAL);

      TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
    }

    return LoadCollectionVocBase(vocbase, collection, status, false);
  }

  // unloaded, load collection
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_document_collection_t* document;

    // set the status to loading
    collection->_status = TRI_VOC_COL_STATUS_LOADING;

    // release the lock on the collection temporarily
    // this will allow other threads to check the collection's
    // status while it is loading (loading may take a long time because of
    // disk activity, index creation etc.)
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    document = TRI_OpenDocumentCollection(vocbase, collection);

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
  TRI_json_t* id = TRI_LookupArrayJson(indexJson, "id");

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
      TRI_InitStringJson(id, idString);
      TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, ij->_list, indexJson);
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
      TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, ij->_list, indexJson);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief free the memory associated with all collections in a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionsVocBase (TRI_vector_pointer_t* collections) {
  size_t i, n;

  n = TRI_LengthVectorPointer(collections);

  for (i = 0; i < n; ++i) {
    TRI_vocbase_col_t* c = static_cast<TRI_vocbase_col_t*>(TRI_AtVectorPointer(collections, i));

    TRI_FreeCollectionVocBase(c);
  }
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
  TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_t), false));

  if (vocbase == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  vocbase->_server             = server;
  vocbase->_type               = type;
  vocbase->_id                 = id;
  vocbase->_path               = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, path);
  vocbase->_name               = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, name);
  vocbase->_authInfoLoaded     = false;
  vocbase->_hasCompactor       = false;
  vocbase->_isOwnAppsDirectory = true;
  vocbase->_replicationApplier = nullptr;
  vocbase->_userStructures     = nullptr;

  vocbase->_oldTransactions    = nullptr;

  // use the defaults provided
  TRI_ApplyVocBaseDefaults(vocbase, defaults);

  vocbase->_cursors = TRI_CreateStoreGeneralCursor();

  if (vocbase->_cursors == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_name);
    TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_path);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  // init usage info
  TRI_InitSpin(&vocbase->_usage._lock);
  vocbase->_usage._refCount  = 0;
  vocbase->_usage._isDeleted = false;

  // init collections
  TRI_InitVectorPointer(&vocbase->_collections, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&vocbase->_deadCollections, TRI_UNKNOWN_MEM_ZONE);

  TRI_InitAssociativePointer(&vocbase->_collectionsById,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCid,
                             HashElementCid,
                             EqualKeyCid,
                             nullptr);

  TRI_InitAssociativePointer(&vocbase->_collectionsByName,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCollectionName,
                             HashElementCollectionName,
                             EqualKeyCollectionName,
                             nullptr);

  TRI_InitAuthInfo(vocbase);

  TRI_InitReadWriteLock(&vocbase->_inventoryLock);
  TRI_InitReadWriteLock(&vocbase->_lock);

  TRI_InitCondition(&vocbase->_compactorCondition);
  TRI_InitCondition(&vocbase->_cleanupCondition);

  TRI_CreateUserStructuresVocBase(vocbase);
  
  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the central parts of a vocbase
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyInitialVocBase (TRI_vocbase_t* vocbase) {
  if (vocbase->_userStructures != nullptr) {
    TRI_FreeUserStructuresVocBase(vocbase);
  }

  // free replication
  if (vocbase->_replicationApplier != nullptr) {
    TRI_FreeReplicationApplier(vocbase->_replicationApplier);
    vocbase->_replicationApplier = nullptr;
  }

  if (vocbase->_oldTransactions == nullptr) {
    delete vocbase->_oldTransactions;
  }

  TRI_DestroyCondition(&vocbase->_cleanupCondition);
  TRI_DestroyCondition(&vocbase->_compactorCondition);

  TRI_DestroyReadWriteLock(&vocbase->_lock);
  TRI_DestroyReadWriteLock(&vocbase->_inventoryLock);

  TRI_DestroyAuthInfo(vocbase);

  TRI_DestroyAssociativePointer(&vocbase->_collectionsByName);
  TRI_DestroyAssociativePointer(&vocbase->_collectionsById);

  TRI_DestroyVectorPointer(&vocbase->_collections);
  TRI_DestroyVectorPointer(&vocbase->_deadCollections);

  TRI_DestroySpin(&vocbase->_usage._lock);

  TRI_FreeStoreGeneralCursor(vocbase->_cursors);

  // free name and path
  TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_path);
  TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_name);
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
    TRI_DestroyInitialVocBase(vocbase);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);
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

  TRI_vector_pointer_t collections;
  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);

  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  // cannot use this vocbase from now on
  TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  // from here on, the vocbase is unusable, i.e. no collections can be created/loaded etc.

  // starts unloading of collections
  for (size_t i = 0;  i < collections._length;  ++i) {
    TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(vocbase->_collections._buffer[i]);

    TRI_UnloadCollectionVocBase(vocbase, collection, true);
  }

  TRI_DestroyVectorPointer(&collections);

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
  for (size_t i = 0;  i < vocbase->_deadCollections._length;  ++i) {
    TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(vocbase->_deadCollections._buffer[i]);

    TRI_FreeCollectionVocBase(collection);
  }

  // free collections
  for (size_t i = 0;  i < vocbase->_collections._length;  ++i) {
    TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(vocbase->_collections._buffer[i]);

    TRI_FreeCollectionVocBase(collection);
  }

  TRI_DestroyCompactorVocBase(vocbase);
  TRI_DestroyInitialVocBase(vocbase);
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

TRI_vector_pointer_t TRI_CollectionsVocBase (TRI_vocbase_t* vocbase) {
  TRI_vector_pointer_t result;

  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

  for (size_t i = 0;  i < vocbase->_collectionsById._nrAlloc;  ++i) {
    TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(vocbase->_collectionsById._table[i]);

    if (found != nullptr) {
      TRI_PushBackVectorPointer(&result, found);
    }
  }

  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns names of all known (document) collections
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_CollectionNamesVocBase (TRI_vocbase_t* vocbase) {
  TRI_vector_string_t result;

  TRI_InitVectorString(&result, TRI_UNKNOWN_MEM_ZONE);

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

  for (size_t i = 0;  i < vocbase->_collectionsById._nrAlloc;  ++i) {
    TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(vocbase->_collectionsById._table[i]);

    if (found != nullptr) {
      char const* name = found->_name;

      if (name != nullptr) {
        TRI_PushBackVectorString(&result, TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, name));
      }
    }
  }

  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

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
  TRI_vector_pointer_t collections;
  TRI_json_t* json;
  size_t i, n;

  json = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

  if (json == nullptr) {
    return nullptr;
  }

  TRI_InitVectorPointer(&collections, TRI_CORE_MEM_ZONE);

  while (! TRI_TryWriteLockReadWriteLock(&vocbase->_inventoryLock)) {
    // cycle on write-lock
    usleep(1000);
  }

  // copy collection pointers into vector so we can work with the copy without
  // the global lock
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  n = collections._length;
  for (i = 0; i < n; ++i) {
    TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(TRI_AtVectorPointer(&collections, i));

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

    TRI_json_t* result = TRI_CreateArray2Json(TRI_CORE_MEM_ZONE, 2);

    if (result != nullptr) {
      TRI_json_t* collectionInfo = TRI_ReadJsonCollectionInfo(collection);

      if (collectionInfo != nullptr) {
        TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, result, "parameters", collectionInfo);

        TRI_json_t* indexesInfo = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

        if (indexesInfo != nullptr) {
          index_json_helper_t ij;
          ij._list    = indexesInfo;
          ij._maxTick = maxTick;

          TRI_IterateJsonIndexesCollectionInfo(collection, &FilterCollectionIndex, &ij);
          TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, result, "indexes", indexesInfo);
        }
      }

      TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, json, result);
    }

    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
  }

  TRI_WriteUnlockReadWriteLock(&vocbase->_inventoryLock);

  TRI_DestroyVectorPointer(&collections);

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
  char* name;

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

  TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));

  if (found == nullptr) {
    name = nullptr;
  }
  else {
    name = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, found->_name);
  }

  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByNameVocBase (TRI_vocbase_t* vocbase,
                                                      char const* name) {
  const char c = *name;

  // if collection name is passed as a stringified id, we'll use the lookupbyid function
  // this is safe because collection names must not start with a digit
  if (c >= '0' && c <= '9') {
    return TRI_LookupCollectionByIdVocBase(vocbase, TRI_UInt64String(name));
  }

  // otherwise we'll look up the collection by name
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByIdVocBase (TRI_vocbase_t* vocbase,
                                                    TRI_voc_cid_t id) {
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a collection by name, optionally creates it
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_FindCollectionByNameOrCreateVocBase (TRI_vocbase_t* vocbase,
                                                            char const* name,
                                                            const TRI_col_type_t type) {
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  if (found != nullptr) {
    return found;
  }
  else {
    // collection not found. now create it
    TRI_vocbase_col_t* collection;
    TRI_col_info_t parameter;

    TRI_InitCollectionInfo(vocbase,
                           &parameter,
                           name,
                           (TRI_col_type_e) type,
                           (TRI_voc_size_t) vocbase->_settings.defaultMaximalSize,
                           nullptr);
    collection = TRI_CreateCollectionVocBase(vocbase, &parameter, 0, true);
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

  TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

  TRI_vocbase_col_t* collection = CreateCollection(vocbase, parameters, cid, writeMarker);

  TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

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

  // added callback for unload
  TRI_CreateBarrierUnloadCollection(&collection->_collection->_barrierList,
                                    collection->_collection,
                                    UnloadCollectionCallback,
                                    collection);

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

  TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // .............................................................................
  // collection already deleted
  // .............................................................................

  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    // mark collection as deleted
    UnregisterCollection(vocbase, collection);

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

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

      TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);
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

        TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);
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

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // collection is loading
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_LOADING) {
    // loop until status changes
    while (1) {
      TRI_vocbase_col_status_e status = collection->_status;

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
      TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

      if (status != TRI_VOC_COL_STATUS_LOADING) {
        break;
      }
      usleep(COLLECTION_STATUS_POLL_INTERVAL);

      TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);
      TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
    }

    // try again with changed status
    return TRI_DropCollectionVocBase(vocbase, collection, writeMarker);
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

      TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

      return res;
    }

    collection->_status = TRI_VOC_COL_STATUS_DELETED;

    UnregisterCollection(vocbase, collection);
    if (writeMarker) {
      WriteDropCollectionMarker(vocbase, collection->_cid);
    }

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);
    
    if (triagens::wal::LogfileManager::instance()->isInRecovery()) {
      DropCollectionCallback(nullptr, collection);
    }
    else {
      // added callback for dropping
      TRI_CreateBarrierDropCollection(&collection->_collection->_barrierList,
                                      collection->_collection,
                                      DropCollectionCallback,
                                      collection);

      // wake up the cleanup thread
      TRI_LockCondition(&vocbase->_cleanupCondition);
      TRI_SignalCondition(&vocbase->_cleanupCondition);
      TRI_UnlockCondition(&vocbase->_cleanupCondition);
    }

    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // upps, unknown status
  // .............................................................................

  else {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    LOG_WARNING("internal error in TRI_DropCollectionVocBase");

    return TRI_set_errno(TRI_ERROR_INTERNAL);
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

  TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

  int res = RenameCollection(vocbase, collection, oldName, newName, writeMarker);

  TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

  TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

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

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_vocbase_col_t const* collection = static_cast<TRI_vocbase_col_t const*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &cid));
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

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

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_vocbase_col_t const* collection = static_cast<TRI_vocbase_col_t const*>(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  if (collection == nullptr) {
    LOG_DEBUG("unknown collection '%s'", name);

    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return nullptr;
  }

  // .............................................................................
  // try to load the collection
  // .............................................................................

  int res = LoadCollectionVocBase(vocbase, const_cast<TRI_vocbase_col_t*>(collection), status);

  return res == TRI_ERROR_NO_ERROR ? const_cast<TRI_vocbase_col_t*>(collection) : nullptr;
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
  TRI_LockSpin(&vocbase->_usage._lock);
  bool isDeleted = vocbase->_usage._isDeleted;
  TRI_UnlockSpin(&vocbase->_usage._lock);

  return isDeleted;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

bool TRI_UseVocBase (TRI_vocbase_t* vocbase) {
  bool result;

  TRI_LockSpin(&vocbase->_usage._lock);
  ++vocbase->_usage._refCount;
  if (vocbase->_usage._isDeleted) {
    result = false;
  }
  else {
    result = true;
  }
  TRI_UnlockSpin(&vocbase->_usage._lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseVocBase (TRI_vocbase_t* vocbase) {
  TRI_LockSpin(&vocbase->_usage._lock);
  TRI_ASSERT(vocbase->_usage._refCount > 0);
  --vocbase->_usage._refCount;
  TRI_UnlockSpin(&vocbase->_usage._lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief marks a database as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropVocBase (TRI_vocbase_t* vocbase) {
  bool result;

  TRI_LockSpin(&vocbase->_usage._lock);
  if (vocbase->_usage._isDeleted) {
    result = false;
  }
  else {
    vocbase->_usage._isDeleted = true;
    result = true;

  }
  TRI_UnlockSpin(&vocbase->_usage._lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether any references are held on a database
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsUsedVocBase (TRI_vocbase_t* vocbase) {
  bool result;

  TRI_LockSpin(&vocbase->_usage._lock);
  result = (vocbase->_usage._refCount > 0);
  TRI_UnlockSpin(&vocbase->_usage._lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database can be removed
////////////////////////////////////////////////////////////////////////////////

bool TRI_CanRemoveVocBase (TRI_vocbase_t* vocbase) {
  bool result;

  TRI_LockSpin(&vocbase->_usage._lock);
  result = (vocbase->_usage._isDeleted && vocbase->_usage._refCount == 0);
  TRI_UnlockSpin(&vocbase->_usage._lock);

  return result;
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
  bool ok;
  char const* ptr;
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is allowed
  for (ptr = name;  *ptr;  ++ptr) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
