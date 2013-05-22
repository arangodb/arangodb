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
#include "VocBase/server-id.h"
#include "VocBase/shadow-data.h"
#include "VocBase/synchroniser.h"
#include "VocBase/transaction.h"

#include "Ahuacatl/ahuacatl-functions.h"
#include "Ahuacatl/ahuacatl-statementlist.h"

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
/// @brief returns the current tick value, without using a lock
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t GetTick (void) {
  return (ServerIdentifier | (CurrentTick << 16));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, without using a lock
////////////////////////////////////////////////////////////////////////////////

static inline void UpdateTick (TRI_voc_tick_t tick) {
  TRI_voc_tick_t s = tick >> 16;

  if (CurrentTick < s) {
    CurrentTick = s;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the memory associated with a collection
////////////////////////////////////////////////////////////////////////////////

static void FreeCollection (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  TRI_DestroyReadWriteLock(&collection->_lock);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a collection name from the global list of collections
///
/// This function is called when a collection is dropped.
////////////////////////////////////////////////////////////////////////////////

static bool UnregisterCollection (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, collection->_name);
  TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsById, &collection->_cid);

  TRI_ASSERT_MAINTAINER(vocbase->_collectionsByName._nrUsed == vocbase->_collectionsById._nrUsed);

  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

static bool UnloadCollectionCallback (TRI_collection_t* col, void* data) {
  TRI_vocbase_col_t* collection;
  TRI_vocbase_t* vocbase;
  TRI_voc_cid_t cid;
  TRI_document_collection_t* document;
  int res;

  collection = data;

  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
  cid = collection->_cid;
  vocbase = collection->_vocbase;

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
  
  if (cid > 0) {
    TRI_RemoveCollectionTransactionContext(vocbase->_transactionContext, cid);
  }

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

static bool DropCollectionCallback (TRI_collection_t* col, void* data) {
  TRI_document_collection_t* document;
  TRI_vocbase_col_t* collection;
  TRI_vocbase_t* vocbase;
  TRI_voc_cid_t cid;
  regmatch_t matches[3];
  regex_t re;
  int res;
  size_t i;

  collection = data;
  cid = 0;

  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_DELETED) {
    LOG_ERROR("someone resurrected the collection '%s'", collection->_name);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
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
      return false;
    }

    cid = collection->_cid;

    document = (TRI_document_collection_t*) collection->_collection;

    res = TRI_CloseDocumentCollection(document);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("failed to close collection '%s': %s",
                collection->_name,
                TRI_last_error());

      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
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

#ifdef _WIN32
      // .........................................................................
      // Just thank your lucky stars that there are only 4 backslashes
      // .........................................................................
      regcomp(&re, "^(.*)\\\\collection-([0-9][0-9]*)$", REG_ICASE | REG_EXTENDED);
#else
      regcomp(&re, "^(.*)/collection-([0-9][0-9]*)$", REG_ICASE | REG_EXTENDED);
#endif
    
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
        if (collection->_vocbase->_removeOnDrop) {
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

    regfree(&re);
  }

  if (cid > 0) {
    TRI_RemoveCollectionTransactionContext(vocbase->_transactionContext, cid);
  }

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
/// @brief get the server id filename
////////////////////////////////////////////////////////////////////////////////

static char* GetServerIdFilename (TRI_vocbase_t* vocbase) {
  char* filename = TRI_Concatenate2File(vocbase->_path, "SERVER");

  return filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read / create the server id on startup
////////////////////////////////////////////////////////////////////////////////

static int ReadServerId (TRI_vocbase_t* vocbase) {
  char* filename;
  int res;

  filename = GetServerIdFilename(vocbase);

  if (filename == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = TRI_ReadServerId(filename);
  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // id file does not yet exist. now create it
    res = TRI_GenerateServerId();

    if (res == TRI_ERROR_NO_ERROR) {
      // id was generated. now save it
      res = TRI_WriteServerId(filename);
    }
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  return res;
}

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

  // create the init object
  TRI_vocbase_col_t init = {
    vocbase,
    (TRI_col_type_t) type,
    cid
  };

  init._status = TRI_VOC_COL_STATUS_CORRUPTED;
  init._collection = NULL;

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
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, collection->_name, collection, false);

  if (found != NULL) {
    LOG_ERROR("duplicate entry for collection name '%s'", collection->_name);

    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    return NULL;
  }

  // check collection identifier (unknown for new born collections)
  found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsById, &cid, collection, false);

  if (found != NULL) {
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, collection->_name);

    LOG_ERROR("duplicate collection identifier %llu for name '%s'",
              (unsigned long long) collection->_cid, collection->_name);

    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
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
  UpdateTick(marker->_tick);
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans a directory and loads all collections
////////////////////////////////////////////////////////////////////////////////

static int ScanPath (TRI_vocbase_t* vocbase, char const* path) {
  TRI_vector_string_t files;
  regmatch_t matches[2];
  regex_t re;
  int res;
  size_t n;
  size_t i;

  files = TRI_FilesDirectory(path);
  n = files._length;

  regcomp(&re, "^collection-([0-9][0-9]*)$", REG_EXTENDED);

  for (i = 0;  i < n;  ++i) {
    char* name;
    char* file;

    name = files._buffer[i];
    assert(name);

    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {

      // do not issue a notice about the "lock" file
      if (! TRI_EqualString(name, "lock")) {
        LOG_DEBUG("ignoring file/directory '%s'", name);
      }

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
        regfree(&re);
        TRI_DestroyVectorString(&files);

        return TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
      }

      // no need to lock as we are scanning
      res = TRI_LoadCollectionInfo(file, &info, true);

      if (res == TRI_ERROR_NO_ERROR) {
        UpdateTick(info._cid);
      }

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_DEBUG("ignoring directory '%s' without valid parameter file '%s'", file, TRI_COL_PARAMETER_FILE);
      }
      else if (info._deleted) {
        // we found a collection that is marked as deleted.
        // it depends on the configuration what will happen with these collections

        if (vocbase->_removeOnDrop) {
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
              regfree(&re);
              TRI_DestroyVectorString(&files);
              TRI_FreeCollectionInfoOptions(&info);
 
              return TRI_set_errno(res);
            }
          } 

          c = AddCollection(vocbase, type, info._name, info._cid, file);
          
          TRI_IterateTicksCollection(file, StartupTickIterator, NULL);

          if (c == NULL) {
            LOG_ERROR("failed to add document collection from '%s'", file);

            TRI_FreeString(TRI_CORE_MEM_ZONE, file);
            regfree(&re);
            TRI_DestroyVectorString(&files);
            TRI_FreeCollectionInfoOptions(&info);

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

static int LoadCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
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
/// Returns true if the name is allowed and false otherwise
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedCollectionName (bool allowSystem, char const* name) {
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
  TRI_LockSpin(&TickLock);
  UpdateTick(tick);
  TRI_UnlockSpin(&TickLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief msyncs a memory block between begin (incl) and end (excl)
////////////////////////////////////////////////////////////////////////////////

bool TRI_msync (int fd, void* mmHandle, char const* begin, char const* end) {
  uintptr_t p = (intptr_t) begin;
  uintptr_t q = (intptr_t) end;
  uintptr_t g = (intptr_t) PageSize;

  char* b = (char*)( (p / g) * g );
  char* e = (char*)( ((q + g - 1) / g) * g );
  int result;

  result = TRI_FlushMMFile(fd, &mmHandle, b, e - b, MS_SYNC);

  if (result != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(result);
    return false;
  }
  return true;
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

    TRI_set_errno(TRI_ERROR_ARANGO_WRONG_VOCBASE_PATH);
    return NULL;
  }

  if (! TRI_IsWritable(path)) {
    // database directory is not writable for the current user... bad luck
    LOG_ERROR("database directory '%s' is not writable for current user", path);

    TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
    return NULL;
  }

  // .............................................................................
  // check that the database is not locked and lock it
  // .............................................................................

  lockFile = TRI_Concatenate2File(path, "lock");
  res = TRI_VerifyLockFile(lockFile);

  if (res == TRI_ERROR_NO_ERROR) {
    LOG_FATAL_AND_EXIT("database is locked, please check the lock file '%s'", lockFile);
  }

  if (TRI_ExistsFile(lockFile)) {
    TRI_UnlinkFile(lockFile);
  }

  res = TRI_CreateLockFile(lockFile);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_FATAL_AND_EXIT("cannot lock the database, please check the lock file '%s': %s", lockFile, TRI_last_error());
  }

  // .............................................................................
  // setup vocbase structure
  // .............................................................................

  vocbase = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_t), false);

  if (vocbase == NULL) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  vocbase->_authInfoLoaded = false;

  vocbase->_cursors = TRI_CreateShadowsGeneralCursor();
  if (vocbase->_cursors == NULL) {
    LOG_FATAL_AND_EXIT("cannot create cursors");
  }

  vocbase->_lockFile = lockFile;
  vocbase->_path = TRI_DuplicateString(path);

  // init AQL functions
  vocbase->_functions = TRI_InitialiseFunctionsAql();

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

  // transactions
  vocbase->_transactionContext = TRI_CreateTransactionContext(vocbase);

  TRI_InitAssociativePointer(&vocbase->_authInfo,
                             TRI_CORE_MEM_ZONE,
                             HashKeyAuthInfo,
                             HashElementAuthInfo,
                             EqualKeyAuthInfo,
                             NULL);

  TRI_InitReadWriteLock(&vocbase->_authInfoLock);
  TRI_InitReadWriteLock(&vocbase->_lock);
  vocbase->_authInfoFlush = true;

  vocbase->_syncWaiters = 0;
  TRI_InitCondition(&vocbase->_syncWaitersCondition);
  TRI_InitCondition(&vocbase->_cleanupCondition);

  // .............................................................................
  // scan directory for collections
  // .............................................................................

  // defaults
  vocbase->_defaultMaximalSize     = TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE;
  vocbase->_defaultWaitForSync     = false; // default sync behavior for new collections
  vocbase->_forceSyncShapes        = true;  // force sync of shape data to disk
  vocbase->_removeOnCompacted      = true;
  vocbase->_removeOnDrop           = true;
 
  if (TRI_ERROR_NO_ERROR != ReadServerId(vocbase)) {
    LOG_FATAL_AND_EXIT("reading/creating server id failed");
  }

  // scan the database path for collections
  // this will create the list of collections and their datafiles, and will also
  // determine the last tick values used
  res = ScanPath(vocbase, vocbase->_path);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativePointer(&vocbase->_collectionsByName);
    TRI_DestroyAssociativePointer(&vocbase->_collectionsById);
    TRI_DestroyVectorPointer(&vocbase->_collections);
    TRI_DestroyVectorPointer(&vocbase->_deadCollections);
    TRI_DestroyLockFile(vocbase->_lockFile);
    TRI_FreeString(TRI_CORE_MEM_ZONE, vocbase->_lockFile);
    TRI_FreeShadowStore(vocbase->_cursors);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);
    TRI_DestroyReadWriteLock(&vocbase->_authInfoLock);
    TRI_DestroyReadWriteLock(&vocbase->_lock);

    return NULL;
  }

  LOG_TRACE("last tick value found: %llu", (unsigned long long) GetTick());  


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
  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  // starts unloading of collections
  for (i = 0;  i < collections._length;  ++i) {
    TRI_vocbase_col_t* collection;

    collection = (TRI_vocbase_col_t*) vocbase->_collections._buffer[i];
    TRI_UnloadCollectionVocBase(vocbase, collection);
  }

  TRI_DestroyVectorPointer(&collections);

  // this will signal the synchroniser and the compactor threads to do one last iteration
  vocbase->_state = 2;

  // wait until synchroniser and compactor are finished
  TRI_JoinThread(&vocbase->_synchroniser);
  TRI_JoinThread(&vocbase->_compactor);

  // this will signal the cleanup thread to do one last iteration
  vocbase->_state = 3;
  TRI_JoinThread(&vocbase->_cleanup);

  // free dead collections (already dropped but pointers still around)
  for (i = 0;  i < vocbase->_deadCollections._length;  ++i) {
    TRI_vocbase_col_t* collection;

    collection = (TRI_vocbase_col_t*) vocbase->_deadCollections._buffer[i];
    TRI_RemoveCollectionTransactionContext(vocbase->_transactionContext, collection->_cid);
    FreeCollection(vocbase, collection);
  }

  // free collections
  for (i = 0;  i < vocbase->_collections._length;  ++i) {
    TRI_vocbase_col_t* collection;

    collection = (TRI_vocbase_col_t*) vocbase->_collections._buffer[i];
    TRI_RemoveCollectionTransactionContext(vocbase->_transactionContext, collection->_cid);
    FreeCollection(vocbase, collection);
  }

  // free the auth info
  TRI_DestroyAuthInfo(vocbase);

  // clear the hashes and vectors
  TRI_DestroyAssociativePointer(&vocbase->_collectionsByName);
  TRI_DestroyAssociativePointer(&vocbase->_collectionsById);
  TRI_DestroyAssociativePointer(&vocbase->_authInfo);

  TRI_DestroyVectorPointer(&vocbase->_collections);
  TRI_DestroyVectorPointer(&vocbase->_deadCollections);

  // free AQL functions
  TRI_FreeFunctionsAql(vocbase->_functions);

  // free the cursors
  TRI_FreeShadowStore(vocbase->_cursors);

  // release lock on database
  TRI_DestroyLockFile(vocbase->_lockFile);
  TRI_FreeString(TRI_CORE_MEM_ZONE, vocbase->_lockFile);

  TRI_FreeTransactionContext(vocbase->_transactionContext);

  // destroy locks
  TRI_DestroyReadWriteLock(&vocbase->_authInfoLock);
  TRI_DestroyReadWriteLock(&vocbase->_lock);
  TRI_DestroyCondition(&vocbase->_syncWaitersCondition);
  TRI_DestroyCondition(&vocbase->_cleanupCondition);

  // free the filename path
  TRI_Free(TRI_CORE_MEM_ZONE, vocbase->_path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief load authentication information
////////////////////////////////////////////////////////////////////////////////

void TRI_LoadAuthInfoVocBase (TRI_vocbase_t* vocbase) {
  vocbase->_authInfoLoaded = TRI_LoadAuthInfo(vocbase);

  TRI_DefaultAuthInfo(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_CollectionsVocBase (TRI_vocbase_t* vocbase) {
  TRI_vector_pointer_t result;
  TRI_vocbase_col_t* found;
  size_t i;

  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

  for (i = 0;  i < vocbase->_collectionsById._nrAlloc;  ++i) {
    found = vocbase->_collectionsById._table[i];

    if (found != NULL) {
      TRI_PushBackVectorPointer(&result, found);
    }
  }

  TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return result;
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

TRI_vocbase_col_t* TRI_LookupCollectionByNameVocBase (TRI_vocbase_t* vocbase, char const* name) {
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
                                                            const TRI_col_type_t type) {
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
                           (TRI_voc_size_t) vocbase->_defaultMaximalSize, 
                           NULL);
    collection = TRI_CreateCollectionVocBase(vocbase, &parameter, 0);
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
                                                TRI_voc_cid_t cid) {
  TRI_vocbase_col_t* collection;
  TRI_collection_t* col;
  TRI_primary_collection_t* primary = NULL;
  TRI_document_collection_t* document;
  TRI_col_type_e type;
  char const* name;
  void const* found;

  assert(parameter);
  name = parameter->_name;

  // check that the name does not contain any strange characters
  if (! TRI_IsAllowedCollectionName(parameter->_isSystem, name)) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);

    return NULL;
  }

  type = (TRI_col_type_e) parameter->_type;

  if (! TRI_IS_DOCUMENT_COLLECTION(type)) {
    LOG_ERROR("unknown collection type: %d", (int) parameter->_type);

    TRI_set_errno(TRI_ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE);
    return NULL;
  }

  TRI_WRITE_LOCK_COLLECTIONS_VOCBASE(vocbase);

  // .............................................................................
  // check that we have a new name
  // .............................................................................

  found = TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name);

  if (found != NULL) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

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
    return NULL;
  }


  collection->_status = TRI_VOC_COL_STATUS_LOADED;
  collection->_collection = primary;
  TRI_CopyString(collection->_path, primary->base._directory, sizeof(collection->_path));

  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnloadCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // cannot unload a corrupted collection
  if (collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
  }

  // a unloaded collection is unloaded
  if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
    return TRI_ERROR_NO_ERROR;
  }

  // a unloading collection is treated as unloaded
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

int TRI_DropCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  int res;

  // mark collection as deleted
  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  // .............................................................................
  // collection already deleted
  // .............................................................................

  if (collection->_status == TRI_VOC_COL_STATUS_DELETED) {
    UnregisterCollection(vocbase, collection);

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
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
      return TRI_set_errno(res);
    }

    // remove dangling .json.tmp file if it exists
    tmpFile = TRI_Concatenate4String(collection->_path, TRI_DIR_SEPARATOR_STR, TRI_COL_PARAMETER_FILE, ".tmp");
    if (tmpFile != NULL) {
      if (TRI_ExistsFile(tmpFile)) {
        TRI_UnlinkFile(tmpFile);
        LOG_WARNING("removing dangling temporary file '%s'", tmpFile);
      }
      TRI_FreeString(TRI_CORE_MEM_ZONE, tmpFile);
    }

    if (! info._deleted) {
      info._deleted = true;

      res = TRI_SaveCollectionInfo(collection->_path, &info, vocbase->_forceSyncProperties);
      TRI_FreeCollectionInfoOptions(&info);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
        return TRI_set_errno(res);
      }
    }

    collection->_status = TRI_VOC_COL_STATUS_DELETED;

    UnregisterCollection(vocbase, collection);

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    DropCollectionCallback(0, collection);

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
      return res;
    }

    collection->_status = TRI_VOC_COL_STATUS_DELETED;
    UnregisterCollection(vocbase, collection);

    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

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
    LOG_WARNING("internal error in TRI_DropCollectionVocBase");

    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection, char const* newName) {
  TRI_col_info_t info;
  void const* found;
  char const* oldName;
  bool isSystem;
  int res;

  // old name should be different
  oldName = collection->_name;

  if (TRI_EqualString(oldName, newName)) {
    return TRI_ERROR_NO_ERROR;
  }

  isSystem = TRI_IsSystemCollectionName(oldName);
  if (isSystem && ! TRI_IsSystemCollectionName(newName)) {
    // a system collection shall not be renamed to a non-system collection name
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }
  else if (! isSystem && TRI_IsSystemCollectionName(newName)) {
    // a non-system collection shall not be renamed to a system collection name
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  if (! TRI_IsAllowedCollectionName(isSystem, newName)) {
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // lock collection because we are going to change the name
  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

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

  if (found != NULL) {
    TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

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

      return TRI_set_errno(res);
    }

    TRI_CopyString(info._name, newName, sizeof(info._name));

    res = TRI_SaveCollectionInfo(collection->_path, &info, vocbase->_forceSyncProperties);
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

  else if (collection->_status == TRI_VOC_COL_STATUS_LOADED || collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    res = TRI_RenameCollection(&collection->_collection->base, newName);

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
  TRI_CopyString(collection->_name, newName, sizeof(collection->_name));

  TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName, newName, CONST_CAST(collection), false);

  TRI_WRITE_UNLOCK_COLLECTIONS_VOCBASE(vocbase);
  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage, loading or manifesting it
////////////////////////////////////////////////////////////////////////////////

int TRI_UseCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  return LoadCollectionVocBase(vocbase, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by id
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByIdVocBase (TRI_vocbase_t* vocbase, const TRI_voc_cid_t cid) {
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

TRI_vocbase_col_t* TRI_UseCollectionByNameVocBase (TRI_vocbase_t* vocbase, char const* name) {
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

void TRI_ReleaseCollectionVocBase (TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection) {
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
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
  // TODO: these two fcalls can probably be removed because we're initialising
  // BasicsC anyway
  TRI_InitialiseHashes();
  TRI_InitialiseRandom();

  TRI_GlobalInitStatementListAql();

  ServerIdentifier = TRI_UInt16Random();
  PageSize = (size_t) getpagesize();

  TRI_InitSpin(&TickLock);

#ifdef TRI_READLINE_VERSION
  LOG_TRACE("%s", "$Revision: READLINE " TRI_READLINE_VERSION " $");
#endif

#ifdef TRI_V8_VERSION
  LOG_TRACE("%s", "$Revision: V8 " TRI_V8_VERSION " $");
#endif

#ifdef TRI_ICU_VERSION
  LOG_TRACE("%s", "$Revision: ICU " TRI_ICU_VERSION " $");
#endif

  LOG_TRACE("sizeof df_header:        %d", (int) sizeof(TRI_df_marker_t));
  LOG_TRACE("sizeof df_header_marker: %d", (int) sizeof(TRI_df_header_marker_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the voc database components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownVocBase () {
  TRI_DestroySpin(&TickLock);

  TRI_ShutdownRandom();
  TRI_ShutdownHashes();
  TRI_GlobalFreeStatementListAql();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
