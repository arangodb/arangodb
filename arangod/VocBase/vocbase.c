////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "vocbase.h"

#include <regex.h>

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/hashes.h"
#include "BasicsC/locks.h"
#include "BasicsC/logging.h"
#include "BasicsC/memory-map.h"
#include "BasicsC/random.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/threads.h"

#include "VocBase/auth.h"
#include "VocBase/barrier.h"
#include "VocBase/cleanup.h"
#include "VocBase/compactor.h"
#include "VocBase/document-collection.h"
#include "VocBase/general-cursor.h"
#include "VocBase/primary-collection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-logger.h"
#include "VocBase/server.h"
#include "VocBase/shadow-data.h"
#include "VocBase/synchroniser.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase-defaults.h"

#ifdef TRI_SKIPLIST_EX
#include "VocBase/index-garbage-collector.h"
#endif

#include "Ahuacatl/ahuacatl-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief auxiliary struct for index iteration
////////////////////////////////////////////////////////////////////////////////

typedef struct index_json_helper_s {
  TRI_json_t*    _list;
  TRI_voc_tick_t _maxTick;
}
index_json_helper_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             DICTIONARY FUNCTOIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the auth info
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAuthInfo (TRI_associative_pointer_t* array, void const* key) {
  char const* k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the auth info
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAuthInfo (TRI_associative_pointer_t* array, void const* element) {
  TRI_vocbase_auth_t const* e = element;

  return TRI_FnvHashString(e->_username);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a auth info and a username
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAuthInfo (TRI_associative_pointer_t* array, void const* key, void const* element) {
  char const* k = (char const*) key;
  TRI_vocbase_auth_t const* e = element;

  return TRI_EqualString(k, e->_username);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyCid (TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_cid_t const* k = key;

  return *k;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection id
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
  TRI_vocbase_col_t const* e = element;
  char const* name = e->_name;

  return TRI_FnvHashString(name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection name and a collection
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
/// @brief removes a collection name from the global list of collections
///
/// This function is called when a collection is dropped.
/// note: the collection must be write-locked for this operation
////////////////////////////////////////////////////////////////////////////////

static bool UnregisterCollection (TRI_vocbase_t* vocbase, 
                                  TRI_vocbase_col_t* collection,
                                  TRI_server_id_t generatingServer) {
  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, collection->_name);
  TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsById, &collection->_cid);

  TRI_ASSERT_MAINTAINER(vocbase->_collectionsByName._nrUsed == vocbase->_collectionsById._nrUsed);

  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  TRI_LogDropCollectionReplication(vocbase, 
                                   collection->_cid, 
                                   collection->_name, 
                                   generatingServer);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

static bool UnloadCollectionCallback (TRI_collection_t* col, void* data) {
  TRI_vocbase_col_t* collection;
  TRI_document_collection_t* document;
  int res;

  collection = data;

  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADING) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return false;
  }

  if (collection->_collection == NULL) {
    collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return true;
  }

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    LOG_ERROR("cannot unload collection '%s' of type '%d'",
              collection->_name,
              (int) collection->_type);

    collection->_status = TRI_VOC_COL_STATUS_LOADED;

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return false;
  }
  
  if (TRI_ContainsBarrierList(&collection->_collection->_barrierList, TRI_BARRIER_ELEMENT) ||
      TRI_ContainsBarrierList(&collection->_collection->_barrierList, TRI_BARRIER_COLLECTION_REPLICATION) ||
      TRI_ContainsBarrierList(&collection->_collection->_barrierList, TRI_BARRIER_COLLECTION_COMPACTION)) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return false;
  }

  document = (TRI_document_collection_t*) collection->_collection;

  res = TRI_CloseDocumentCollection(document);

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
  collection->_collection = NULL;
  
  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

static bool DropCollectionCallback (TRI_collection_t* col, 
                                    void* data) {
  TRI_document_collection_t* document;
  TRI_vocbase_col_t* collection;
  TRI_vocbase_t* vocbase;
  regmatch_t matches[3];
  regex_t re;
  int res;
  size_t i;

  collection = data;

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


  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_DELETED) {
    LOG_ERROR("someone resurrected the collection '%s'", collection->_name);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
    regfree(&re);

    return false;
  }

  // .............................................................................
  // unload collection
  // .............................................................................

  if (collection->_collection != NULL) {
    if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
      LOG_ERROR("cannot drop collection '%s' of type %d",
                collection->_name,
                (int) collection->_type);

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
      regfree(&re);

      return false;
    }

    document = (TRI_document_collection_t*) collection->_collection;

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

    collection->_collection = NULL;
  }

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  // .............................................................................
  // remove from list of collections
  // .............................................................................

  vocbase = collection->_vocbase;

  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  for (i = 0;  i < vocbase->_collections._length;  ++i) {
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

      res = TRI_RenameFile(collection->_path, newFilename);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_ERROR("cannot rename dropped collection '%s' from '%s' to '%s'",
                  collection->_name,
                  collection->_path,
                  newFilename);
      }
      else {
        if (collection->_vocbase->_settings.removeOnDrop) {
          LOG_DEBUG("wiping dropped collection '%s' from disk",
                    collection->_name);

          res = TRI_RemoveDirectory(newFilename);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_ERROR("cannot wipe dropped collecton '%s' from disk: %s",
                      collection->_name,
                      TRI_last_error());
          }
        }
        else {
          LOG_DEBUG("renamed dropped collection '%s' from '%s' to '%s'",
                    collection->_name,
                    collection->_path,
                    newFilename);
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
///
/// Caller must hold TRI_WRITE_LOCK_COLLECTIONS_VOCBASE.
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* AddCollection (TRI_vocbase_t* vocbase,
                                         TRI_col_type_e type,
                                         char const* name,
                                         TRI_voc_cid_t cid,
                                         char const* path) {
  void const* found;
  TRI_vocbase_col_t* collection;
  int res;

  // create the init object
  TRI_vocbase_col_t init = {
    vocbase,
    (TRI_col_type_t) type,
    cid
  };

  init._status      = TRI_VOC_COL_STATUS_CORRUPTED;
  init._collection  = NULL;

  // default flags: everything is allowed
  init._canDrop     = true;
  init._canRename   = true;
  init._canUnload   = true;
 
  // check for special system collection names 
  if (TRI_IsSystemNameCollection(name)) {
    // a few system collections have special behavior
    if (TRI_EqualString(name, TRI_COL_NAME_ENDPOINTS) ||
        TRI_EqualString(name, TRI_COL_NAME_REPLICATION) ||
        TRI_EqualString(name, TRI_COL_NAME_TRANSACTION) ||
        TRI_EqualString(name, TRI_COL_NAME_USERS)) {
      // these collections cannot be dropped or renamed
      init._canDrop   = false;
      init._canRename = false;

      // the replication collection cannot be unloaded manually)
      // (this would make the server hang)
      init._canUnload = ! TRI_EqualString(name, TRI_COL_NAME_REPLICATION);
    }
  }

  TRI_CopyString(init._name, name, sizeof(init._name));

  if (path == NULL) {
    init._path[0] = '\0';
  }
  else {
    TRI_CopyString(init._path, path, sizeof(collection->_path));
  }

  // create a new proxy
  collection = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_col_t), false);

  if (collection == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  memcpy(collection, &init, sizeof(TRI_vocbase_col_t));

  // check name
  res = TRI_InsertKeyAssociativePointer2(&vocbase->_collectionsByName, collection->_name, collection, &found);
  
  if (found != NULL) {
    LOG_ERROR("duplicate entry for collection name '%s'", collection->_name);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);

    return NULL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // OOM. this might have happend AFTER insertion
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, collection->_name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    TRI_set_errno(res);

    return NULL;
  }

  // check collection identifier (unknown for new born collections)
  res = TRI_InsertKeyAssociativePointer2(&vocbase->_collectionsById, &cid, collection, &found);
  
  if (found != NULL) {
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, collection->_name);

    LOG_ERROR("duplicate collection identifier %llu for name '%s'",
              (unsigned long long) collection->_cid, collection->_name);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);

    return NULL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // OOM. this might have happend AFTER insertion
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsById, &cid);
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, collection->_name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);

    TRI_set_errno(res);

    return NULL;
  }

  TRI_InitReadWriteLock(&collection->_lock);

  // this needs TRI_WRITE_LOCK_COLLECTIONS_VOCBASE
  TRI_PushBackVectorPointer(&vocbase->_collections, collection);
  return collection;
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
                                 TRI_datafile_t* datafile, 
                                 bool journal) {
  TRI_FastUpdateTickServer(marker->_tick);
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans a directory and loads all collections
////////////////////////////////////////////////////////////////////////////////

static int ScanPath (TRI_vocbase_t* vocbase, 
                     char const* path,
                     const bool iterateMarkers) {
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

  for (i = 0;  i < n;  ++i) {
    char* name;
    char* file;

    name = files._buffer[i];
    assert(name != NULL);

    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {
      // no match, ignore this file
      continue;
    }

    file = TRI_Concatenate2File(path, name);

    if (file == NULL) {
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
        // it depends on the configuration what will happen with these collections

        if (vocbase->_settings.removeOnDrop) {
          // deleted collections should be removed on startup. this is the default
          LOG_DEBUG("collection '%s' was deleted, wiping it", name);

          res = TRI_RemoveDirectory(file);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_WARNING("cannot wipe deleted collection: %s", TRI_last_error());
          }
        }
        else {
          // deleted collections shout not be removed on startup
          char* newFile;
          char* tmp1;
          char* tmp2;

          char const* first = name + matches[1].rm_so;
          size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

          tmp1 = TRI_DuplicateString2(first, firstLen);
          tmp2 = TRI_Concatenate2String("deleted-", tmp1);

          TRI_FreeString(TRI_CORE_MEM_ZONE, tmp1);

          newFile = TRI_Concatenate2File(path, tmp2);

          TRI_FreeString(TRI_CORE_MEM_ZONE, tmp2);

          LOG_WARNING("collection '%s' was deleted, renaming it to '%s'", name, newFile);

          res = TRI_RenameFile(file, newFile);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_WARNING("cannot rename deleted collection: %s", TRI_last_error());
          }

          TRI_FreeString(TRI_CORE_MEM_ZONE, newFile);
        }
      }
      else {
        // we found a collection that is still active
        TRI_col_type_e type = info._type;

        if (TRI_IS_DOCUMENT_COLLECTION(type)) {
          TRI_vocbase_col_t* c;

          if (info._version < TRI_COL_VERSION) {
            LOG_INFO("upgrading collection '%s'", info._name);

            res = TRI_UpgradeCollection(vocbase, file, &info);

            if (res != TRI_ERROR_NO_ERROR) {
              LOG_ERROR("upgrading collection '%s' failed.", info._name);
            
              TRI_FreeString(TRI_CORE_MEM_ZONE, file);
              TRI_DestroyVectorString(&files);
              TRI_FreeCollectionInfoOptions(&info);
              regfree(&re);
 
              return TRI_set_errno(res);
            }
          } 

          c = AddCollection(vocbase, type, info._name, info._cid, file);
       
          if (iterateMarkers) {   
            // iterating markers may be time-consuming. we'll only do it if
            // we have to
            TRI_IterateTicksCollection(file, StartupTickIterator, NULL);
          }

          if (c == NULL) {
            LOG_ERROR("failed to add document collection from '%s'", file);

            TRI_FreeString(TRI_CORE_MEM_ZONE, file);
            TRI_DestroyVectorString(&files);
            TRI_FreeCollectionInfoOptions(&info);
            regfree(&re);

            return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
          }

          c->_status = TRI_VOC_COL_STATUS_UNLOADED;

          LOG_DEBUG("added document collection from '%s'", file);
        }
        else {
          LOG_DEBUG("skipping collection of unknown type %d", (int) type);
        }
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
                                  TRI_vocbase_col_t* collection) {
  TRI_col_type_e type;
  
  // .............................................................................
  // read lock
  // .............................................................................

  // check if the collection is already loaded
  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

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

  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // someone else loaded the collection, release the WRITE lock and try again
  if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return LoadCollectionVocBase(vocbase, collection);
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

    return LoadCollectionVocBase(vocbase, collection);
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

  // unloaded, load collection
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    type = (TRI_col_type_e) collection->_type;

    if (TRI_IS_DOCUMENT_COLLECTION(type)) {
      TRI_document_collection_t* document;

      document = TRI_OpenDocumentCollection(vocbase, collection->_path);

      if (document == NULL) {
        collection->_status = TRI_VOC_COL_STATUS_CORRUPTED;

        TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
        return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
      }

      collection->_collection = &document->base;
      collection->_status = TRI_VOC_COL_STATUS_LOADED;
      TRI_CopyString(collection->_path, document->base.base._directory, sizeof(collection->_path));

      // release the WRITE lock and try again
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      return LoadCollectionVocBase(vocbase, collection);
    }
    else {
      LOG_ERROR("unknown collection type %d for '%s'", (int) type, collection->_name);

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
      return TRI_set_errno(TRI_ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE);
    }
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
  TRI_json_t* indexJson;
  TRI_json_t* id;
  char* error = NULL;
  index_json_helper_t* ij = (index_json_helper_t*) data;

  indexJson = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, &error);

  if (error != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, error);
  }

  if (indexJson == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // compare index id with tick value
  id = TRI_LookupArrayJson(indexJson, "id");

  // index id is numeric
  if (id != NULL && id->_type == TRI_JSON_NUMBER) {
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
    TRI_vocbase_col_t* c = TRI_AtVectorPointer(collections, i);

    TRI_FreeCollectionVocBase(c);
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
/// @brief opens an existing database, scans all collections
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_OpenVocBase (TRI_server_t* server,
                                char const* path, 
                                TRI_voc_tick_t id,
                                char const* name,
                                TRI_vocbase_defaults_t const* defaults,
                                bool iterateMarkers) {
  TRI_vocbase_t* vocbase;
  int res;

  assert(name != NULL);
  assert(path != NULL);
  assert(defaults != NULL);
  
  // .............................................................................
  // setup vocbase structure
  // .............................................................................

  vocbase = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_t), false);

  if (vocbase == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return NULL;
  }

  vocbase->_id               = id;
  vocbase->_path             = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, path);
  vocbase->_name             = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, name); 
  vocbase->_authInfoLoaded   = false;

  // use the defaults provided
  TRI_ApplyVocBaseDefaults(vocbase, defaults);
  
  // init AQL functions
  vocbase->_functions = TRI_CreateFunctionsAql();

  if (vocbase->_functions == NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_path);
    TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return NULL;
  }
  
  // init cursors
  vocbase->_cursors = TRI_CreateShadowsGeneralCursor();

  if (vocbase->_cursors == NULL) {
    TRI_FreeFunctionsAql(vocbase->_functions);
    TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_path);
    TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return NULL;
  }

  // init usage info
  TRI_InitSpin(&vocbase->_usage._lock);
  vocbase->_usage._refCount  = 0;
  vocbase->_usage._isDeleted = false;

  TRI_InitCompactorVocBase(vocbase);

  // init collections
  TRI_InitVectorPointer(&vocbase->_collections, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&vocbase->_deadCollections, TRI_UNKNOWN_MEM_ZONE);

  TRI_InitAssociativePointer(&vocbase->_collectionsById,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCid,
                             HashElementCid,
                             EqualKeyCid,
                             NULL);

  TRI_InitAssociativePointer(&vocbase->_collectionsByName,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCollectionName,
                             HashElementCollectionName,
                             EqualKeyCollectionName,
                             NULL);

  TRI_InitAssociativePointer(&vocbase->_authInfo,
                             TRI_CORE_MEM_ZONE,
                             HashKeyAuthInfo,
                             HashElementAuthInfo,
                             EqualKeyAuthInfo,
                             NULL);
  
  TRI_InitReadWriteLock(&vocbase->_inventoryLock);
  TRI_InitReadWriteLock(&vocbase->_authInfoLock);
  TRI_InitReadWriteLock(&vocbase->_lock);
  vocbase->_authInfoFlush = true;

  vocbase->_syncWaiters = 0;
  TRI_InitCondition(&vocbase->_syncWaitersCondition);
  TRI_InitCondition(&vocbase->_cleanupCondition);

  // .............................................................................
  // scan directory for collections
  // .............................................................................

 
  // scan the database path for collections
  // this will create the list of collections and their datafiles, and will also
  // determine the last tick values used (if iterateMarkers is true)

  res = ScanPath(vocbase, vocbase->_path, iterateMarkers);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeFunctionsAql(vocbase->_functions);
    TRI_DestroyCompactorVocBase(vocbase);
    TRI_DestroyAssociativePointer(&vocbase->_collectionsByName);
    TRI_DestroyAssociativePointer(&vocbase->_collectionsById);
    TRI_DestroyVectorPointer(&vocbase->_collections);
    TRI_DestroyVectorPointer(&vocbase->_deadCollections);
    TRI_FreeString(TRI_CORE_MEM_ZONE, vocbase->_path);
    TRI_FreeString(TRI_CORE_MEM_ZONE, vocbase->_name);
    TRI_FreeShadowStore(vocbase->_cursors);
    TRI_DestroyReadWriteLock(&vocbase->_authInfoLock);
    TRI_DestroyReadWriteLock(&vocbase->_lock);
    TRI_DestroySpin(&vocbase->_usage._lock);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);
    TRI_set_errno(res);

    return NULL;
  }


  // .............................................................................
  // vocbase is now active
  // .............................................................................

  vocbase->_state = 1;

  // .............................................................................
  // start helper threads
  // .............................................................................

  // start synchroniser thread
  TRI_InitThread(&vocbase->_synchroniser);
  TRI_StartThread(&vocbase->_synchroniser, "[synchroniser]", TRI_SynchroniserVocBase, vocbase);

  // start compactor thread
  TRI_InitThread(&vocbase->_compactor);
  TRI_StartThread(&vocbase->_compactor, "[compactor]", TRI_CompactorVocBase, vocbase);

  // start cleanup thread
  TRI_InitThread(&vocbase->_cleanup);
  TRI_StartThread(&vocbase->_cleanup, "[cleanup]", TRI_CleanupVocBase, vocbase);

#ifdef TRI_SKIPLIST_EX
  TRI_InitThread(&(vocbase->_indexGC));
  TRI_StartThread(&(vocbase->_indexGC), "[index_garbage_collector]", TRI_IndexGCVocBase, vocbase);
#endif

  vocbase->_replicationLogger = TRI_CreateReplicationLogger(vocbase);

  if (vocbase->_replicationLogger == NULL) {
    // TODO
    LOG_FATAL_AND_EXIT("initialising replication logger for database '%s' failed", name);
  }

  if (vocbase->_replicationLogger->_configuration._autoStart) {
    if (server->_disableReplicationLoggers) {
      LOG_INFO("replication logger explicitly deactivated for database '%s'", name);
    }
    else {
      res = TRI_StartReplicationLogger(vocbase->_replicationLogger);

      if (res != TRI_ERROR_NO_ERROR) {
        // TODO
        LOG_FATAL_AND_EXIT("unable to start replication logger for database '%s'", name);
      }
    }
  }
  
  vocbase->_replicationApplier = TRI_CreateReplicationApplier(vocbase);

  if (vocbase->_replicationApplier == NULL) {
    // TODO
    LOG_FATAL_AND_EXIT("initialising replication applier for database '%s' failed", name);
  }
 
  if (vocbase->_replicationApplier->_configuration._autoStart) {
    if (server->_disableReplicationAppliers) {
      LOG_INFO("replication applier explicitly deactivated for database '%s'", name);
    }
    else {
      res = TRI_StartReplicationApplier(vocbase->_replicationApplier, 0, false);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("unable to start replication applier for database '%s': %s", 
                    name, 
                    TRI_errno_string(res));
      }
    }
  }

  // we are done
  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a database and all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocBase (TRI_vocbase_t* vocbase) {
  TRI_vector_pointer_t collections;
  size_t i;
  
  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);

  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);
  // cannot use this vocbase from now on
  TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  // from here on, the vocbase is unusable, i.e. no collections can be created/loaded etc.

  // starts unloading of collections
  for (i = 0;  i < collections._length;  ++i) {
    TRI_vocbase_col_t* collection;

    collection = (TRI_vocbase_col_t*) vocbase->_collections._buffer[i];
    TRI_UnloadCollectionVocBase(vocbase, collection);
  }

  TRI_DestroyVectorPointer(&collections);
 
  // stop replication
  TRI_StopReplicationApplier(vocbase->_replicationApplier, false); 
  TRI_StopReplicationLogger(vocbase->_replicationLogger); 
  
  // this will signal the synchroniser and the compactor threads to do one last iteration
  vocbase->_state = 2;

#ifdef TRI_SKIPLIST_EX
  // wait for the index garbage collector to finish what ever it is doing
  TRI_JoinThread(&vocbase->_indexGC);
#endif  
  
  
  // wait until synchroniser and compactor are finished
  TRI_JoinThread(&vocbase->_synchroniser);
  TRI_JoinThread(&vocbase->_compactor);

  // this will signal the cleanup thread to do one last iteration
  vocbase->_state = 3;
  TRI_JoinThread(&vocbase->_cleanup);

  // free replication  
  TRI_FreeReplicationApplier(vocbase->_replicationApplier);
  vocbase->_replicationApplier = NULL;
  
  TRI_FreeReplicationLogger(vocbase->_replicationLogger);
  vocbase->_replicationLogger = NULL;

  // free dead collections (already dropped but pointers still around)
  for (i = 0;  i < vocbase->_deadCollections._length;  ++i) {
    TRI_vocbase_col_t* collection;

    collection = (TRI_vocbase_col_t*) vocbase->_deadCollections._buffer[i];

    TRI_FreeCollectionVocBase(collection);
  }

  // free collections
  for (i = 0;  i < vocbase->_collections._length;  ++i) {
    TRI_vocbase_col_t* collection;

    collection = (TRI_vocbase_col_t*) vocbase->_collections._buffer[i];

    TRI_FreeCollectionVocBase(collection);
  }

  // free the auth info
  TRI_DestroyAuthInfo(vocbase);

  // clear the hashes and vectors
  TRI_DestroyAssociativePointer(&vocbase->_collectionsByName);
  TRI_DestroyAssociativePointer(&vocbase->_collectionsById);
  TRI_DestroyAssociativePointer(&vocbase->_authInfo);

  TRI_DestroyVectorPointer(&vocbase->_collections);
  TRI_DestroyVectorPointer(&vocbase->_deadCollections);

  TRI_DestroyCompactorVocBase(vocbase);

  // free AQL functions
  TRI_FreeFunctionsAql(vocbase->_functions);

  // free the cursors
  TRI_FreeShadowStore(vocbase->_cursors);
  
  // destroy locks
  TRI_DestroySpin(&vocbase->_usage._lock);
  TRI_DestroyReadWriteLock(&vocbase->_inventoryLock);
  TRI_DestroyReadWriteLock(&vocbase->_authInfoLock);
  TRI_DestroyReadWriteLock(&vocbase->_lock);
  TRI_DestroyCondition(&vocbase->_syncWaitersCondition);
  TRI_DestroyCondition(&vocbase->_cleanupCondition);

  // free name and path
  TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_path);
  TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief load authentication information
////////////////////////////////////////////////////////////////////////////////

void TRI_LoadAuthInfoVocBase (TRI_vocbase_t* vocbase) {
  vocbase->_authInfoLoaded = TRI_LoadAuthInfo(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_CollectionsVocBase (TRI_vocbase_t* vocbase) {
  TRI_vector_pointer_t result;
  size_t i;

  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

  for (i = 0;  i < vocbase->_collectionsById._nrAlloc;  ++i) {
    TRI_vocbase_col_t* found;

    found = vocbase->_collectionsById._table[i];

    if (found != NULL) {
      TRI_PushBackVectorPointer(&result, found);
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

  if (json == NULL) {
    return NULL;
  }

  TRI_InitVectorPointer(&collections, TRI_CORE_MEM_ZONE);

  TRI_WriteLockReadWriteLock(&vocbase->_inventoryLock);

  // copy collection pointers into vector so we can work with the copy without
  // the global lock 
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  n = collections._length;
  for (i = 0; i < n; ++i) {
    TRI_vocbase_col_t* collection;
    TRI_json_t* result;
     
    collection = TRI_AtVectorPointer(&collections, i);

    TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

    if (collection->_status == TRI_VOC_COL_STATUS_DELETED ||
        collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
      // we do not need to care about deleted or corrupted collections
      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
      continue;
    }
        
    if (collection->_cid >= maxTick) {
      // collection is too new
      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
      continue;
    }

    // check if we want this collection
    if (filter != NULL && ! filter(collection, data)) {
      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
      continue;
    }

    result = TRI_CreateArray2Json(TRI_CORE_MEM_ZONE, 2);

    if (result != NULL) {
      TRI_json_t* collectionInfo;
      TRI_json_t* indexesInfo;

      collectionInfo = TRI_ReadJsonCollectionInfo(collection);

      if (collectionInfo != NULL) {
        TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, result, "parameters", collectionInfo);

        indexesInfo = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

        if (indexesInfo != NULL) {
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
/// @brief gets a collection name by a collection id
///
/// The name is fetched under a lock to make this thread-safe. Returns NULL if
/// the collection does not exist. It is the caller's responsibility to free the
/// name returned
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetCollectionNameByIdVocBase (TRI_vocbase_t* vocbase,
                                        const TRI_voc_cid_t id) {
  TRI_vocbase_col_t* found;
  char* name;

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

  found = CONST_CAST(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));

  if (found == NULL) {
    name = NULL;
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
  TRI_vocbase_col_t* found;
  const char c = *name;
  
  // if collection name is passed as a stringified id, we'll use the lookupbyid function
  // this is safe because collection names must not start with a digit
  if (c >= '0' && c <= '9') {
    return TRI_LookupCollectionByIdVocBase(vocbase, TRI_UInt64String(name));
  }

  // otherwise we'll look up the collection by name
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  found = CONST_CAST(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByIdVocBase (TRI_vocbase_t* vocbase, TRI_voc_cid_t id) {
  TRI_vocbase_col_t* found;
  
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  found = CONST_CAST(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a collection by name, optionally creates it
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_FindCollectionByNameOrCreateVocBase (TRI_vocbase_t* vocbase,
                                                            char const* name,
                                                            const TRI_col_type_t type,
                                                            TRI_server_id_t generatingServer) {
  TRI_vocbase_col_t* found;
  
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  found = CONST_CAST(TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  if (found != NULL) {
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
                           NULL);
    collection = TRI_CreateCollectionVocBase(vocbase, &parameter, 0, generatingServer);
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
                                                TRI_col_info_t* parameter,
                                                TRI_voc_cid_t cid,
                                                TRI_server_id_t generatingServer) {
  TRI_vocbase_col_t* collection;
  TRI_collection_t* col;
  TRI_primary_collection_t* primary = NULL;
  TRI_document_collection_t* document;
  TRI_json_t* json;
  TRI_col_type_e type;
  char const* name;
  void const* found;

  assert(parameter);
  name = parameter->_name;

  // check that the name does not contain any strange characters
  if (! TRI_IsAllowedNameCollection(parameter->_isSystem, name)) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);

    return NULL;
  }

  type = (TRI_col_type_e) parameter->_type;

  if (! TRI_IS_DOCUMENT_COLLECTION(type)) {
    LOG_ERROR("unknown collection type: %d", (int) parameter->_type);

    TRI_set_errno(TRI_ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE);
    return NULL;
  }

  TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  // .............................................................................
  // check that we have a new name
  // .............................................................................

  found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found != NULL) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    LOG_DEBUG("collection named '%s' already exists", name);

    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    return NULL;
  }

  // .............................................................................
  // ok, construct the collection
  // .............................................................................

  document = TRI_CreateDocumentCollection(vocbase, vocbase->_path, parameter, cid);

  if (document == NULL) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    return NULL;
  }

  primary = &document->base;

  col = &primary->base;

  // add collection container -- however note that the compactor is added later which could fail!
  collection = AddCollection(vocbase,
                             col->_info._type,
                             col->_info._name,
                             col->_info._cid,
                             col->_directory);

  if (collection == NULL) {
    if (TRI_IS_DOCUMENT_COLLECTION(type)) {
      TRI_CloseDocumentCollection(document);
      TRI_FreeDocumentCollection(document);
    }

    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    return NULL;
  }


  collection->_status = TRI_VOC_COL_STATUS_LOADED;
  collection->_collection = primary;
  TRI_CopyString(collection->_path, 
                 primary->base._directory, 
                 sizeof(collection->_path));
  
  // grab a read-lock on the collection so no one else can unload it
  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  // release the lock on the list of collections
  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

  // replicate and finally unlock the collection
  json = TRI_CreateJsonCollectionInfo(&col->_info);
  TRI_LogCreateCollectionReplication(vocbase, 
                                     col->_info._cid, 
                                     col->_info._name, 
                                     json,
                                     generatingServer);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnloadCollectionVocBase (TRI_vocbase_t* vocbase, 
                                 TRI_vocbase_col_t* collection) {
  if (! collection->_canUnload) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN); 
  }

  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

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
                                    &collection->_collection->base,
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
                               TRI_server_id_t generatingServer) {
  int res;
  
  if (! collection->_canDrop) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN); 
  }
  
  TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

  // mark collection as deleted
  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // .............................................................................
  // collection already deleted
  // .............................................................................

  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    UnregisterCollection(vocbase, collection, generatingServer);

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // collection is unloaded
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_col_info_t info;
    char* tmpFile;

    res = TRI_LoadCollectionInfo(collection->_path, &info, true);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);
      return TRI_set_errno(res);
    }

    // remove dangling .json.tmp file if it exists
    tmpFile = TRI_Concatenate4String(collection->_path, TRI_DIR_SEPARATOR_STR, TRI_VOC_PARAMETER_FILE, ".tmp");
    if (tmpFile != NULL) {
      if (TRI_ExistsFile(tmpFile)) {
        TRI_UnlinkFile(tmpFile);
        LOG_DEBUG("removing dangling temporary file '%s'", tmpFile);
      }
      TRI_FreeString(TRI_CORE_MEM_ZONE, tmpFile);
    }

    if (! info._deleted) {
      info._deleted = true;

      res = TRI_SaveCollectionInfo(collection->_path, &info, vocbase->_settings.forceSyncProperties);
      TRI_FreeCollectionInfoOptions(&info);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

        TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);
        return TRI_set_errno(res);
      }
    }

    collection->_status = TRI_VOC_COL_STATUS_DELETED;

    UnregisterCollection(vocbase, collection, generatingServer);

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    DropCollectionCallback(0, collection);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // collection is loaded
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_LOADED || collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    collection->_collection->base._info._deleted = true;

    res = TRI_UpdateCollectionInfo(vocbase, &collection->_collection->base, 0);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

      return res;
    }

    collection->_status = TRI_VOC_COL_STATUS_DELETED;
    UnregisterCollection(vocbase, collection, generatingServer);

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    // added callback for dropping
    TRI_CreateBarrierDropCollection(&collection->_collection->_barrierList,
                                    &collection->_collection->base,
                                    DropCollectionCallback,
                                    collection);

    // wake up the cleanup thread
    TRI_LockCondition(&vocbase->_cleanupCondition);
    TRI_SignalCondition(&vocbase->_cleanupCondition);
    TRI_UnlockCondition(&vocbase->_cleanupCondition);

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
                                 bool override,
                                 TRI_server_id_t generatingServer) {
  TRI_col_info_t info;
  void const* found;
  char* oldName;
  int res;
  
  if (! collection->_canRename) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN); 
  }

  // lock collection because we are going to copy its current name
  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  // old name should be different
  oldName = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, collection->_name);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (oldName == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (TRI_EqualString(oldName, newName)) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

    return TRI_ERROR_NO_ERROR;
  }

  if (! override) {
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

  // lock collection because we are going to change the name
  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // this must be done after the collection lock
  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  // cannot rename a corrupted collection
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

    return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
  }

  // cannot rename a deleted collection
  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

    return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  // check if the new name is unused
  found = (void*) TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, newName);

  if (found != NULL) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

    return TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
  }

  // .............................................................................
  // collection is unloaded
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    res = TRI_LoadCollectionInfo(collection->_path, &info, true);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

      return TRI_set_errno(res);
    }

    TRI_CopyString(info._name, newName, sizeof(info._name));

    TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

    res = TRI_SaveCollectionInfo(collection->_path, &info, vocbase->_settings.forceSyncProperties);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    TRI_FreeCollectionInfoOptions(&info);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

      return TRI_set_errno(res);
    }
  }

  // .............................................................................
  // collection is loaded
  // .............................................................................

  else if (collection->_status == TRI_VOC_COL_STATUS_LOADED || 
           collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

    res = TRI_RenameCollection(&collection->_collection->base, newName);

    TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

      return TRI_set_errno(res);
    }
  }

  // .............................................................................
  // upps, unknown status
  // .............................................................................

  else {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // .............................................................................
  // rename and release locks
  // .............................................................................

  TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

  TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, oldName);
  TRI_CopyString(collection->_name, newName, sizeof(collection->_name));
 
  // this shouldn't fail, as we removed an element above so adding one should be ok
  TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, newName, CONST_CAST(collection), false);

  TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  // stay inside the outer lock to protect against unloading
  TRI_LogRenameCollectionReplication(vocbase, collection->_cid, newName, generatingServer);
  
  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    
  TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage, loading or manifesting it
////////////////////////////////////////////////////////////////////////////////

int TRI_UseCollectionVocBase (TRI_vocbase_t* vocbase, 
                              TRI_vocbase_col_t* collection) {
  return LoadCollectionVocBase(vocbase, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by id
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByIdVocBase (TRI_vocbase_t* vocbase, 
                                                 const TRI_voc_cid_t cid) {
  TRI_vocbase_col_t const* collection;
  int res;

  // .............................................................................
  // check that we have an existing name
  // .............................................................................

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  collection = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &cid);
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  if (collection == NULL) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return NULL;
  }

  // .............................................................................
  // try to load the collection
  // .............................................................................

  res = LoadCollectionVocBase(vocbase, CONST_CAST(collection));

  return res == TRI_ERROR_NO_ERROR ? CONST_CAST(collection) : NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByNameVocBase (TRI_vocbase_t* vocbase, 
                                                   char const* name) {
  TRI_vocbase_col_t const* collection;
  int res;

  // .............................................................................
  // check that we have an existing name
  // .............................................................................

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  collection = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  if (collection == NULL) {
    LOG_DEBUG("unknown collection '%s'", name);

    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return NULL;
  }

  // .............................................................................
  // try to load the collection
  // .............................................................................

  res = LoadCollectionVocBase(vocbase, CONST_CAST(collection));

  return res == TRI_ERROR_NO_ERROR ? CONST_CAST(collection) : NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a (document) collection from usage
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseCollectionVocBase (TRI_vocbase_t* vocbase, 
                                   TRI_vocbase_col_t* collection) {
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

bool TRI_UseVocBase (TRI_vocbase_t* vocbase) {
  bool result;

  TRI_LockSpin(&vocbase->_usage._lock);
  if (vocbase->_usage._isDeleted) {
    result = false;
  }
  else {
    ++vocbase->_usage._refCount;
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
