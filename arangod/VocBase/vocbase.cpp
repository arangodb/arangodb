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
#include "Basics/Logger.h"
#include "Basics/memory-map.h"
#include "Basics/random.h"
#include "Basics/tri-strings.h"
#include "Basics/threads.h"
#include "Basics/Exceptions.h"
#include "Utils/CollectionKeysRepository.h"
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

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep interval used when polling for a loading collection's status
////////////////////////////////////////////////////////////////////////////////

#define COLLECTION_STATUS_POLL_INTERVAL (1000 * 10)

static std::atomic<TRI_voc_tick_t> QueryId(1);

static std::atomic<bool> ThrowCollectionNotLoaded(false);

////////////////////////////////////////////////////////////////////////////////
/// @brief states for TRI_DropCollectionVocBase()
////////////////////////////////////////////////////////////////////////////////

enum DropState {
  DROP_EXIT,    // drop done, nothing else to do
  DROP_AGAIN,   // drop not done, must try again
  DROP_PERFORM  // drop done, must perform actual cleanup routine
};

////////////////////////////////////////////////////////////////////////////////
/// @brief collection constructor
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t::TRI_vocbase_col_t(TRI_vocbase_t* vocbase,
                                     TRI_col_type_e type,
                                     std::string const& name, TRI_voc_cid_t cid,
                                     std::string const& path)
    : _vocbase(vocbase),
      _cid(cid),
      _planId(0),
      _type(static_cast<TRI_col_type_t>(type)),
      _lock(),
      _internalVersion(0),
      _status(TRI_VOC_COL_STATUS_CORRUPTED),
      _collection(nullptr),
      _path(path),
      _dbName(vocbase->_name),
      _name(name),
      _isLocal(true),
      _canDrop(true),
      _canUnload(true),
      _canRename(true) {
  // check for special system collection names
  if (TRI_IsSystemNameCollection(name.c_str())) {
    // a few system collections have special behavior
    if (TRI_EqualString(name.c_str(), TRI_COL_NAME_USERS) ||
        TRI_IsPrefixString(name.c_str(), TRI_COL_NAME_STATISTICS)) {
      // these collections cannot be dropped or renamed
      _canDrop = false;
      _canRename = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief collection destructor
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t::~TRI_vocbase_col_t() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyCid(TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_cid_t const* k = static_cast<TRI_voc_cid_t const*>(key);

  return *k;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementCid(TRI_associative_pointer_t* array,
                               void const* element) {
  TRI_vocbase_col_t const* e = static_cast<TRI_vocbase_col_t const*>(element);

  return e->_cid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection id and a collection
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyCid(TRI_associative_pointer_t* array, void const* key,
                        void const* element) {
  TRI_voc_cid_t const* k = static_cast<TRI_voc_cid_t const*>(key);
  TRI_vocbase_col_t const* e = static_cast<TRI_vocbase_col_t const*>(element);

  return *k == e->_cid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyCollectionName(TRI_associative_pointer_t* array,
                                      void const* key) {
  char const* k = (char const*)key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementCollectionName(TRI_associative_pointer_t* array,
                                          void const* element) {
  TRI_vocbase_col_t const* e = static_cast<TRI_vocbase_col_t const*>(element);
  std::string const colName(e->name());

  return TRI_FnvHashString(colName.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection name and a collection
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyCollectionName(TRI_associative_pointer_t* array,
                                   void const* key, void const* element) {
  char const* k = static_cast<char const*>(key);
  TRI_vocbase_col_t const* e = static_cast<TRI_vocbase_col_t const*>(element);

  std::string const colName(e->name());
  return TRI_EqualString(k, colName.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write a drop collection marker into the log
////////////////////////////////////////////////////////////////////////////////

static int WriteDropCollectionMarker(TRI_vocbase_t* vocbase,
                                     TRI_voc_cid_t collectionId) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::wal::DropCollectionMarker marker(vocbase->_id, collectionId);
    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "could not save collection drop marker in log: " << TRI_errno_string(res);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a collection name from the global list of collections
///
/// This function is called when a collection is dropped.
/// note: the collection must be write-locked for this operation
////////////////////////////////////////////////////////////////////////////////

static bool UnregisterCollection(TRI_vocbase_t* vocbase,
                                 TRI_vocbase_col_t* collection) {
  TRI_ASSERT(collection != nullptr);
  std::string const colName(collection->name());

  WRITE_LOCKER(writeLocker, vocbase->_collectionsLock);

  // pre-condition
  TRI_ASSERT(vocbase->_collectionsByName._nrUsed ==
             vocbase->_collectionsById._nrUsed);

  // only if we find the collection by its id, we can delete it by name
  if (TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsById,
                                      &collection->_cid) != nullptr) {
    // this is because someone else might have created a new collection with the
    // same name,
    // but with a different id
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName,
                                    colName.c_str());
  }

  // post-condition
  TRI_ASSERT(vocbase->_collectionsByName._nrUsed ==
                       vocbase->_collectionsById._nrUsed);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

static bool UnloadCollectionCallback(TRI_collection_t* col, void* data) {
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

  if (ditches->contains(arangodb::Ditch::TRI_DITCH_DOCUMENT) ||
      ditches->contains(arangodb::Ditch::TRI_DITCH_REPLICATION) ||
      ditches->contains(arangodb::Ditch::TRI_DITCH_COMPACTION)) {
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    // still some ditches left...
    // as the cleanup thread has already popped the unload ditch from the
    // ditches list,
    // we need to insert a new one to really executed the unload
    TRI_UnloadCollectionVocBase(collection->_collection->_vocbase, collection,
                                false);

    return false;
  }

  TRI_document_collection_t* document = collection->_collection;

  TRI_ASSERT(document != nullptr);

  int res = TRI_CloseDocumentCollection(document, true);

  if (res != TRI_ERROR_NO_ERROR) {
    std::string const colName(collection->name());
    LOG(ERR) << "failed to close collection '" << colName.c_str() << "': " << TRI_last_error();

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

static bool DropCollectionCallback(TRI_collection_t* col, void* data) {
  TRI_vocbase_t* vocbase;
  regmatch_t matches[4];
  regex_t re;
  int res;

  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(data);
  std::string const name(collection->name());

#ifdef _WIN32
  // .........................................................................
  // Just thank your lucky stars that there are only 4 backslashes
  // .........................................................................
  res = regcomp(&re, "^(.*)\\\\collection-([0-9][0-9]*)(-[0-9]+)?$",
                REG_ICASE | REG_EXTENDED);
#else
  res =
      regcomp(&re, "^(.*)/collection-([0-9][0-9]*)(-[0-9]+)?$", REG_ICASE | REG_EXTENDED);
#endif

  if (res != 0) {
    LOG(ERR) << "unable to complile regular expression";

    return false;
  }

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_DELETED) {
    LOG(ERR) << "someone resurrected the collection '" << name.c_str() << "'";
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
      LOG(ERR) << "failed to close collection '" << name.c_str() << "': " << TRI_last_error();

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
    WRITE_LOCKER(writeLocker, vocbase->_collectionsLock);

    auto it = std::find(vocbase->_collections.begin(),
                        vocbase->_collections.end(), collection);

    if (it != vocbase->_collections.end()) {
      vocbase->_collections.erase(it);
    }

    // we need to clean up the pointers later so we insert it into this vector
    vocbase->_deadCollections.emplace_back(collection);
  }

  // .............................................................................
  // rename collection directory
  // .............................................................................

  if (!collection->path().empty()) {
    int regExpResult;

    regExpResult = regexec(&re, collection->pathc_str(),
                           sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (regExpResult == 0) {
      char const* first = collection->pathc_str() + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

      char const* second = collection->pathc_str() + matches[2].rm_so;
      size_t secondLen = matches[2].rm_eo - matches[2].rm_so;

      char* tmp1;
      char* tmp2;
      char* tmp3;

      char* newFilename;

      tmp1 = TRI_DuplicateString(first, firstLen);
      tmp2 = TRI_DuplicateString(second, secondLen);
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
      res = TRI_RenameFile(collection->pathc_str(), newFilename);

      LOG(TRACE) << "renaming collection directory from '" << collection->pathc_str() << "' to '" << newFilename << "'";

      if (res != TRI_ERROR_NO_ERROR) {
        LOG(ERR) << "cannot rename dropped collection '" << name.c_str() << "' from '" << collection->pathc_str() << "' to '" << newFilename << "': " << TRI_errno_string(res);
      } else {
        LOG(DEBUG) << "wiping dropped collection '" << name.c_str() << "' from disk";

        res = TRI_RemoveDirectory(newFilename);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(ERR) << "cannot wipe dropped collection '" << name.c_str() << "' from disk: " << TRI_errno_string(res);
        }
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, newFilename);
    } else {
      LOG(ERR) << "cannot rename dropped collection '" << name.c_str() << "': unknown path '" << collection->pathc_str() << "'";
    }
  }

  regfree(&re);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new collection
///
/// Caller must hold _collectionsLock in write mode
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* AddCollection(TRI_vocbase_t* vocbase,
                                        TRI_col_type_e type, char const* name,
                                        TRI_voc_cid_t cid, char const* path) {
  // create a new proxy
  auto collection =
      std::make_unique<TRI_vocbase_col_t>(vocbase, type, name, cid, path);

  TRI_ASSERT(collection != nullptr);

  // check name
  void const* found;
  int res = TRI_InsertKeyAssociativePointer2(&vocbase->_collectionsByName, name,
                                             collection.get(), &found);

  if (found != nullptr) {
    LOG(ERR) << "duplicate entry for collection name '" << name << "'";
    LOG(ERR) << "collection id " << cid << " has same name as already added collection " << static_cast<TRI_vocbase_col_t const*>(found)->_cid;

    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);

    return nullptr;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // OOM. this might have happened AFTER insertion
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, name);
    TRI_set_errno(res);

    return nullptr;
  }

  // check collection identifier
  TRI_ASSERT(collection->_cid == cid);
  res = TRI_InsertKeyAssociativePointer2(&vocbase->_collectionsById, &cid,
                                         collection.get(), &found);

  if (found != nullptr) {
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, name);

    LOG(ERR) << "duplicate collection identifier " << collection->_cid << " for name '" << name << "'";

    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);

    return nullptr;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // OOM. this might have happend AFTER insertion
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsById, &cid);
    TRI_RemoveKeyAssociativePointer(&vocbase->_collectionsByName, name);
    TRI_set_errno(res);

    return nullptr;
  }

  TRI_ASSERT(vocbase->_collectionsByName._nrUsed ==
                       vocbase->_collectionsById._nrUsed);

  // this needs the write lock on _collectionsLock
  // TODO: if the following goes wrong, we still have the collection added into
  // the associative arrays...
  vocbase->_collections.emplace_back(collection.get());

  return collection.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* CreateCollection(
    TRI_vocbase_t* vocbase, arangodb::VocbaseCollectionInfo& parameters,
    TRI_voc_cid_t& cid, bool writeMarker, VPackBuilder& builder) {
  TRI_ASSERT(!builder.isClosed());
  std::string name = parameters.name();

  WRITE_LOCKER(writeLocker, vocbase->_collectionsLock);

  try {
    // reserve room for the new collection
    vocbase->_collections.reserve(vocbase->_collections.size() + 1);
    vocbase->_deadCollections.reserve(vocbase->_deadCollections.size() + 1);
  } catch (...) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // .............................................................................
  // check that we have a new name
  // .............................................................................

  void const* found = TRI_LookupByKeyAssociativePointer(
      &vocbase->_collectionsByName, name.c_str());

  if (found != nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    return nullptr;
  }

  // .............................................................................
  // ok, construct the collection
  // .............................................................................

  TRI_document_collection_t* document =
      TRI_CreateDocumentCollection(vocbase, vocbase->_path, parameters, cid);

  if (document == nullptr) {
    return nullptr;
  }

  TRI_collection_t* col = document;

  // add collection container
  TRI_vocbase_col_t* collection = nullptr;

  try {
    collection =
        AddCollection(vocbase, col->_info.type(), col->_info.namec_str(),
                      col->_info.id(), col->_directory);
  } catch (...) {
    // if an exception is caught, collection will be a nullptr
  }

  if (collection == nullptr) {
    TRI_CloseDocumentCollection(document, false);
    TRI_FreeDocumentCollection(document);
    // TODO: does the collection directory need to be removed?
    return nullptr;
  }

  if (parameters.planId() > 0) {
    collection->_planId = parameters.planId();
    col->_info.setPlanId(parameters.planId());
  }

  // cid might have been assigned
  cid = col->_info.id();

  collection->_status = TRI_VOC_COL_STATUS_LOADED;
  collection->_collection = document;

  if (writeMarker) {
    TRI_CreateVelocyPackCollectionInfo(col->_info, builder);
  }

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

static int RenameCollection(TRI_vocbase_t* vocbase,
                            TRI_vocbase_col_t* collection, char const* oldName,
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
    WRITE_LOCKER(writeLocker, vocbase->_collectionsLock);

    // check if the new name is unused
    void const* found = TRI_LookupByKeyAssociativePointer(
        &vocbase->_collectionsByName, newName);

    if (found != nullptr) {
      return TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    }

    // .............................................................................
    // collection is unloaded
    // .............................................................................

    else if (collection->_status == TRI_VOC_COL_STATUS_UNLOADED) {
      try {
        arangodb::VocbaseCollectionInfo info =
            arangodb::VocbaseCollectionInfo::fromFile(collection->pathc_str(),
                                                      vocbase, newName, true);

        int res = info.saveToFile(collection->pathc_str(),
                                  vocbase->_settings.forceSyncProperties);

        if (res != TRI_ERROR_NO_ERROR) {
          return TRI_set_errno(res);
        }

      } catch (arangodb::basics::Exception const& e) {
        return TRI_set_errno(e.code());
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

    collection->_name = newName;

    // this shouldn't fail, as we removed an element above so adding one should
    // be ok
    found = TRI_InsertKeyAssociativePointer(&vocbase->_collectionsByName,
                                            newName, collection, false);
    TRI_ASSERT(found == nullptr);

    TRI_ASSERT(vocbase->_collectionsByName._nrUsed ==
                         vocbase->_collectionsById._nrUsed);
  }  // _colllectionsLock

  // to prevent caching returning now invalid old collection name in db's
  // NamedPropertyAccessor,
  // i.e. db.<old-collection-name>
  collection->_internalVersion++;

  // invalidate all entries for the two collections
  arangodb::aql::QueryCache::instance()->invalidate(
      vocbase, std::vector<std::string>{oldName, newName});

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief this iterator is called on startup for journal and compactor file
/// of a collection
/// it will check the ticks of all markers and update the internal tick
/// counter accordingly. this is done so we'll not re-assign an already used
/// tick value
////////////////////////////////////////////////////////////////////////////////

static bool StartupTickIterator(TRI_df_marker_t const* marker, void* data,
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

static int ScanPath(TRI_vocbase_t* vocbase, char const* path, bool isUpgrade,
                    bool iterateMarkers) {
  regmatch_t matches[3];
  regex_t re;

  int res = regcomp(&re, "^collection-([0-9][0-9]*)(-[0-9]+)?$", REG_EXTENDED | REG_NOSUB);

  if (res != 0) {
    LOG(ERR) << "unable to compile regular expression";

    return res;
  }

  std::vector<std::string> files = TRI_FilesDirectory(path);

  if (iterateMarkers) {
    LOG(TRACE) << "scanning all collection markers in database '" << vocbase->_name;
  }

  for (auto const& name : files) {
    TRI_ASSERT(!name.empty());

    if (regexec(&re, name.c_str(), sizeof(matches) / sizeof(matches[0]),
                matches, 0) != 0) {
      // no match, ignore this file
      continue;
    }

    char* filePtr = TRI_Concatenate2File(path, name.c_str());

    if (filePtr == nullptr) {
      LOG(FATAL) << "out of memory"; FATAL_ERROR_EXIT();
    }

    std::string file = filePtr;
    TRI_FreeString(TRI_CORE_MEM_ZONE, filePtr);

    if (TRI_IsDirectory(file.c_str())) {
      if (!TRI_IsWritable(file.c_str())) {
        // the collection directory we found is not writable for the current
        // user
        // this can cause serious trouble so we will abort the server start if
        // we
        // encounter this situation
        LOG(ERR) << "database subdirectory '" << file.c_str() << "' is not writable for current user";

        regfree(&re);

        return TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
      }

      int res = TRI_ERROR_NO_ERROR;

      try {
        arangodb::VocbaseCollectionInfo info =
            arangodb::VocbaseCollectionInfo::fromFile(file.c_str(), vocbase,
                                                      "",  // Name is unused
                                                      true);
        TRI_UpdateTickServer(info.id());

        if (info.deleted()) {
          // we found a collection that is marked as deleted.
          // deleted collections should be removed on startup. this is the
          // default
          LOG(DEBUG) << "collection '" << name.c_str() << "' was deleted, wiping it";

          res = TRI_RemoveDirectory(file.c_str());

          if (res != TRI_ERROR_NO_ERROR) {
            LOG(WARN) << "cannot wipe deleted collection: " << TRI_last_error();
          }
        } else {
          // we found a collection that is still active
          TRI_col_type_e type = info.type();

          if (info.version() < TRI_COL_VERSION) {
            // collection is too "old"

            if (!isUpgrade) {
              LOG(ERR) << "collection '" << info.namec_str() << "' has a too old version. Please start the server with the --upgrade option.";

              regfree(&re);

              return TRI_set_errno(res);
            } else {
              if (info.version() < TRI_COL_VERSION_13) {
                LOG(ERR) << "collection '" << info.namec_str() << "' is too old to be upgraded with this ArangoDB version.";
                res = TRI_ERROR_ARANGO_ILLEGAL_STATE;
              } else {
                LOG(INFO) << "upgrading collection '" << info.namec_str() << "'";
                res = TRI_ERROR_NO_ERROR;
              }

              if (res == TRI_ERROR_NO_ERROR &&
                  info.version() < TRI_COL_VERSION_20) {
                LOG(ERR) << "format of collection '" << info.namec_str() << "' is too old";

                regfree(&re);

                return TRI_set_errno(res);
              }
            }
          }

          TRI_vocbase_col_t* c = nullptr;

          try {
            c = AddCollection(vocbase, type, info.namec_str(), info.id(),
                              file.c_str());
          } catch (...) {
            // if we caught an exception, c is still a nullptr
          }

          if (c == nullptr) {
            LOG(ERR) << "failed to add document collection from '" << file.c_str() << "'";

            regfree(&re);

            return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
          }

          c->_planId = info.planId();
          c->_status = TRI_VOC_COL_STATUS_UNLOADED;

          if (iterateMarkers) {
            // iterating markers may be time-consuming. we'll only do it if
            // we have to
            TRI_voc_tick_t tick;
            TRI_IterateTicksCollection(file.c_str(), StartupTickIterator,
                                       &tick);

            TRI_UpdateTickServer(tick);
          }

          LOG(DEBUG) << "added document collection '" << info.namec_str() << "' from '" << file.c_str() << "'";
        }

      } catch (arangodb::basics::Exception const& e) {
        char* tmpfile = TRI_Concatenate2File(file.c_str(), ".tmp");

        if (TRI_ExistsFile(tmpfile)) {
          LOG(TRACE) << "ignoring temporary directory '" << tmpfile << "'";
          TRI_Free(TRI_CORE_MEM_ZONE, tmpfile);
          // temp file still exists. this means the collection was not created
          // fully
          // and needs to be ignored
          continue;  // ignore this directory
        }

        TRI_Free(TRI_CORE_MEM_ZONE, tmpfile);
        res = e.code();

        LOG(ERR) << "cannot read collection info file in directory '" << file.c_str() << "': " << TRI_errno_string(res);
        regfree(&re);
        return TRI_set_errno(res);
      }
    } else {
      LOG(DEBUG) << "ignoring non-directory '" << file.c_str() << "'";
    }
  }

  regfree(&re);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads an existing (document) collection
///
/// Note that this will READ lock the collection. You have to release the
/// collection lock by yourself.
////////////////////////////////////////////////////////////////////////////////

static int LoadCollectionVocBase(TRI_vocbase_t* vocbase,
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
    if (collection->_collection->ditches()->contains(
            arangodb::Ditch::TRI_DITCH_COLLECTION_DROP)) {
      // drop call going on, we must abort
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      // someone requested the collection to be dropped, so it's not there
      // anymore
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

      TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
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

    TRI_document_collection_t* document =
        TRI_OpenDocumentCollection(vocbase, collection, IGNORE_DATAFILE_ERRORS);

    // lock again the adjust the status
    TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

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

    // release the WRITE lock and try again
    TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

    return LoadCollectionVocBase(vocbase, collection, status, false);
  }

  std::string const colName(collection->name());
  LOG(ERR) << "unknown collection status " << collection->_status << " for '" << colName.c_str() << "'";

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);
  return TRI_set_errno(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a (document) collection
////////////////////////////////////////////////////////////////////////////////

static int DropCollection(TRI_vocbase_t* vocbase, TRI_vocbase_col_t* collection,
                          bool writeMarker, DropState& state) {
  state = DROP_EXIT;
  std::string const colName(collection->name());

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  arangodb::aql::QueryCache::instance()->invalidate(vocbase, colName.c_str());

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
    try {
      arangodb::VocbaseCollectionInfo info =
          arangodb::VocbaseCollectionInfo::fromFile(collection->pathc_str(),
                                                    collection->vocbase(),
                                                    colName.c_str(), true);
      if (!info.deleted()) {
        info.setDeleted(true);

        // we don't need to fsync if we are in the recovery phase
        bool doSync =
            (vocbase->_settings.forceSyncProperties &&
             !arangodb::wal::LogfileManager::instance()->isInRecovery());

        int res = info.saveToFile(collection->pathc_str(), doSync);

        if (res != TRI_ERROR_NO_ERROR) {
          TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

          return TRI_set_errno(res);
        }
      }

    } catch (arangodb::basics::Exception const& e) {
      TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

      return TRI_set_errno(e.code());
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

  else if (collection->_status == TRI_VOC_COL_STATUS_LOADED ||
           collection->_status == TRI_VOC_COL_STATUS_UNLOADING) {
    collection->_collection->_info.setDeleted(true);

    bool doSync = (vocbase->_settings.forceSyncProperties &&
                   !arangodb::wal::LogfileManager::instance()->isInRecovery());
    VPackSlice slice;
    int res = TRI_UpdateCollectionInfo(vocbase, collection->_collection, slice,
                                       doSync);

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

    LOG(WARN) << "internal error in TRI_DropCollectionVocBase";

    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if collection _trx is present and build a set of entries
/// with it. this is done to ensure compatibility with old datafiles
////////////////////////////////////////////////////////////////////////////////

static bool ScanTrxCallback(TRI_doc_mptr_t const* mptr,
                            TRI_document_collection_t* document, void* data) {
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

static int ScanTrxCollection(TRI_vocbase_t* vocbase) {
  TRI_vocbase_col_t* collection =
      TRI_LookupCollectionByNameVocBase(vocbase, TRI_COL_NAME_TRANSACTION);

  if (collection == nullptr) {
    // collection not found, no problem - seems to be a newer ArangoDB version
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_INTERNAL;

  {
    arangodb::SingleCollectionReadOnlyTransaction trx(
        new arangodb::StandaloneTransactionContext(), vocbase,
        collection->_cid);

    res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_DocumentIteratorDocumentCollection(&trx, trx.documentCollection(),
                                           vocbase, &ScanTrxCallback);

    trx.finish(res);
  }

  // don't need the collection anymore, so unload it
  TRI_UnloadCollectionVocBase(vocbase, collection, true);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-abc\.ending$/, where abc is
/// a number, and type and ending are arbitrary letters
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetNumericFilenamePart(char const* filename) {
  char const* pos1 = strrchr(filename, '.');

  if (pos1 == nullptr) {
    return 0;
  }

  char const* pos2 = strrchr(filename, '-');

  if (pos2 == nullptr || pos2 > pos1) {
    return 0;
  }

  return TRI_UInt64String2(pos2 + 1, pos1 - pos2 - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort datafile filenames on startup
////////////////////////////////////////////////////////////////////////////////

static bool FilenameStringComparator(std::string const& lhs,
                                     std::string const& rhs) {
  uint64_t const numLeft = GetNumericFilenamePart(lhs.c_str());
  uint64_t const numRight = GetNumericFilenamePart(rhs.c_str());
  return numLeft < numRight;
}

void TRI_vocbase_col_t::toVelocyPack(VPackBuilder& builder, bool includeIndexes,
                                     TRI_voc_tick_t maxTick) {
  TRI_ASSERT(!builder.isClosed());
  char* filename = TRI_Concatenate2File(_path.c_str(), TRI_VOC_PARAMETER_FILE);
  std::string path = std::string(filename, strlen(filename));

  std::shared_ptr<VPackBuilder> fileInfoBuilder =
      arangodb::basics::VelocyPackHelper::velocyPackFromFile(path);
  builder.add("parameters", fileInfoBuilder->slice());
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (includeIndexes) {
    builder.add("indexes", VPackValue(VPackValueType::Array));
    toVelocyPackIndexes(builder, maxTick);
    builder.close();
  }
}

std::shared_ptr<VPackBuilder> TRI_vocbase_col_t::toVelocyPack(
    bool includeIndexes, TRI_voc_tick_t maxTick) {
  auto builder = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder b(builder.get());
    toVelocyPack(*builder, includeIndexes, maxTick);
  }

  return builder;
}

void TRI_vocbase_col_t::toVelocyPackIndexes(VPackBuilder& builder,
                                            TRI_voc_tick_t maxTick) {
  TRI_ASSERT(!builder.isClosed());
  regex_t re;

  if (regcomp(&re, "^index-[0-9][0-9]*\\.json$", REG_EXTENDED | REG_NOSUB) !=
      0) {
    LOG(ERR) << "unable to compile regular expression";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::vector<std::string> files = TRI_FilesDirectory(_path.c_str());

  // sort by index id
  std::sort(files.begin(), files.end(), FilenameStringComparator);

  for (auto const& file : files) {
    if (regexec(&re, file.c_str(), (size_t)0, nullptr, 0) == 0) {
      // TODO: fix memleak
      char* fqn = TRI_Concatenate2File(_path.c_str(), file.c_str());
      std::string path = std::string(fqn, strlen(fqn));
      std::shared_ptr<VPackBuilder> indexVPack =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(path);
      TRI_FreeString(TRI_CORE_MEM_ZONE, fqn);

      VPackSlice const indexSlice = indexVPack->slice();
      VPackSlice const id = indexSlice.get("id");

      if (id.isNumber()) {
        uint64_t iid = id.getNumericValue<uint64_t>();
        if (iid <= static_cast<uint64_t>(maxTick)) {
          // convert "id" to string
          VPackBuilder toMerge;
          {
            VPackObjectBuilder b(&toMerge);
            char* idString = TRI_StringUInt64(iid);
            toMerge.add("id", VPackValue(idString));
          }
          VPackBuilder mergedBuilder =
              VPackCollection::merge(indexSlice, toMerge.slice(), false);
          builder.add(mergedBuilder.slice());
        }
      } else if (id.isString()) {
        std::string data = id.copyString();
        uint64_t iid = TRI_UInt64String2(data.c_str(), data.length());
        if (iid <= static_cast<uint64_t>(maxTick)) {
          builder.add(indexSlice);
        }
      }
    }
  }

  regfree(&re);
}

std::shared_ptr<VPackBuilder> TRI_vocbase_col_t::toVelocyPackIndexes(
    TRI_voc_tick_t maxTick) {
  auto builder = std::make_shared<VPackBuilder>();
  builder->openArray();
  toVelocyPackIndexes(*builder, maxTick);
  builder->close();
  return builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vocbase object, without threads and some other attributes
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_CreateInitialVocBase(
    TRI_server_t* server, TRI_vocbase_type_e type, char const* path,
    TRI_voc_tick_t id, char const* name,
    TRI_vocbase_defaults_t const* defaults) {
  try {
    auto vocbase =
        std::make_unique<TRI_vocbase_t>(server, type, path, id, name, defaults);

    return vocbase.release();
  } catch (...) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing database, scans all collections
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_OpenVocBase(TRI_server_t* server, char const* path,
                               TRI_voc_tick_t id, char const* name,
                               TRI_vocbase_defaults_t const* defaults,
                               bool isUpgrade, bool iterateMarkers) {
  TRI_ASSERT(name != nullptr);
  TRI_ASSERT(path != nullptr);
  TRI_ASSERT(defaults != nullptr);

  TRI_vocbase_t* vocbase = TRI_CreateInitialVocBase(
      server, TRI_VOCBASE_TYPE_NORMAL, path, id, name, defaults);

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

  vocbase->_state = (sig_atomic_t)TRI_VOCBASE_STATE_NORMAL;

  // .............................................................................
  // start helper threads
  // .............................................................................

  // start cleanup thread
  TRI_InitThread(&vocbase->_cleanup);
  TRI_StartThread(&vocbase->_cleanup, nullptr, "Cleanup", TRI_CleanupVocBase,
                  vocbase);

  vocbase->_replicationApplier = TRI_CreateReplicationApplier(server, vocbase);

  if (vocbase->_replicationApplier == nullptr) {
    LOG(FATAL) << "initializing replication applier for database '" << vocbase->_name << "' failed: " << TRI_last_error(); FATAL_ERROR_EXIT();
  }

  // we are done
  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a database and all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocBase(TRI_vocbase_t* vocbase) {
  // stop replication
  if (vocbase->_replicationApplier != nullptr) {
    vocbase->_replicationApplier->stop(false);
  }

  // mark all cursors as deleted so underlying collections can be freed soon
  if (vocbase->_cursorRepository != nullptr) {
    vocbase->_cursorRepository->garbageCollect(true);
  }

  // mark all collection keys as deleted so underlying collections can be freed
  // soon
  if (vocbase->_collectionKeys != nullptr) {
    vocbase->_collectionKeys->garbageCollect(true);
  }

  std::vector<TRI_vocbase_col_t*> collections;

  {
    WRITE_LOCKER(writeLocker, vocbase->_collectionsLock);
    collections = vocbase->_collections;
  }

  // from here on, the vocbase is unusable, i.e. no collections can be
  // created/loaded etc.

  // starts unloading of collections
  for (auto& collection : collections) {
    TRI_UnloadCollectionVocBase(vocbase, collection, true);
  }

  // this will signal the synchronizer and the compactor threads to do one last
  // iteration
  vocbase->_state = (sig_atomic_t)TRI_VOCBASE_STATE_SHUTDOWN_COMPACTOR;

  TRI_LockCondition(&vocbase->_compactorCondition);
  TRI_SignalCondition(&vocbase->_compactorCondition);
  TRI_UnlockCondition(&vocbase->_compactorCondition);

  if (vocbase->_hasCompactor) {
    int res = TRI_JoinThread(&vocbase->_compactor);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "unable to join compactor thread: " << TRI_errno_string(res);
    }
  }

  // this will signal the cleanup thread to do one last iteration
  vocbase->_state = (sig_atomic_t)TRI_VOCBASE_STATE_SHUTDOWN_CLEANUP;

  TRI_LockCondition(&vocbase->_cleanupCondition);
  TRI_SignalCondition(&vocbase->_cleanupCondition);
  TRI_UnlockCondition(&vocbase->_cleanupCondition);

  int res = TRI_JoinThread(&vocbase->_cleanup);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "unable to join cleanup thread: " << TRI_errno_string(res);
  }

  // free dead collections (already dropped but pointers still around)
  for (auto& collection : vocbase->_deadCollections) {
    delete collection;
  }

  // free collections
  for (auto& collection : vocbase->_collections) {
    delete collection;
  }

  TRI_DestroyCompactorVocBase(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the compactor thread
////////////////////////////////////////////////////////////////////////////////

void TRI_StartCompactorVocBase(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(!vocbase->_hasCompactor);

  LOG(TRACE) << "starting compactor for database '" << vocbase->_name << "'";
  // start compactor thread
  TRI_InitThread(&vocbase->_compactor);
  TRI_StartThread(&vocbase->_compactor, nullptr, "Compactor",
                  TRI_CompactorVocBase, vocbase);
  vocbase->_hasCompactor = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the compactor thread
////////////////////////////////////////////////////////////////////////////////

int TRI_StopCompactorVocBase(TRI_vocbase_t* vocbase) {
  if (vocbase->_hasCompactor) {
    vocbase->_hasCompactor = false;

    LOG(TRACE) << "stopping compactor for database '" << vocbase->_name << "'";
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

std::vector<TRI_vocbase_col_t*> TRI_CollectionsVocBase(TRI_vocbase_t* vocbase) {
  std::vector<TRI_vocbase_col_t*> result;

  READ_LOCKER(readLocker, vocbase->_collectionsLock);

  for (size_t i = 0; i < vocbase->_collectionsById._nrAlloc; ++i) {
    TRI_vocbase_col_t* found =
        static_cast<TRI_vocbase_col_t*>(vocbase->_collectionsById._table[i]);

    if (found != nullptr) {
      result.emplace_back(found);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns names of all known (document) collections
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TRI_CollectionNamesVocBase(TRI_vocbase_t* vocbase) {
  std::vector<std::string> result;

  READ_LOCKER(readLocker, vocbase->_collectionsLock);

  for (size_t i = 0; i < vocbase->_collectionsById._nrAlloc; ++i) {
    TRI_vocbase_col_t* found =
        static_cast<TRI_vocbase_col_t*>(vocbase->_collectionsById._table[i]);

    if (found != nullptr) {
      result.emplace_back(found->name());
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections with their parameters
/// and indexes, up to a specific tick value
/// while the collections are iterated over, there will be a global lock so
/// that there will be consistent view of collections & their properties
/// The list of collections will be sorted if sort function is given
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> TRI_InventoryCollectionsVocBase(
    TRI_vocbase_t* vocbase, TRI_voc_tick_t maxTick,
    bool (*filter)(TRI_vocbase_col_t*, void*), void* data, bool shouldSort,
    std::function<bool(TRI_vocbase_col_t*, TRI_vocbase_col_t*)> sortCallback) {
  std::vector<TRI_vocbase_col_t*> collections;

  // cycle on write-lock
  WRITE_LOCKER_EVENTUAL(writeLock, vocbase->_inventoryLock, 1000);

  // copy collection pointers into vector so we can work with the copy without
  // the global lock
  {
    READ_LOCKER(readLocker, vocbase->_collectionsLock);
    collections = vocbase->_collections;
  }

  if (shouldSort && collections.size() > 1) {
    std::sort(collections.begin(), collections.end(), sortCallback);
  }

  auto builder = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder b(builder.get());

    for (auto& collection : collections) {
      READ_LOCKER(readLocker, collection->_lock);

      if (collection->_status == TRI_VOC_COL_STATUS_DELETED ||
          collection->_status == TRI_VOC_COL_STATUS_CORRUPTED) {
        // we do not need to care about deleted or corrupted collections
        continue;
      }

      if (collection->_cid > maxTick) {
        // collection is too new
        continue;
      }

      // check if we want this collection
      if (filter != nullptr && !filter(collection, data)) {
        continue;
      }

      VPackObjectBuilder b(builder.get());
      collection->toVelocyPack(*builder, true, maxTick);
    }
  }
  return builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a translation of a collection status
////////////////////////////////////////////////////////////////////////////////

char const* TRI_GetStatusStringCollectionVocBase(
    TRI_vocbase_col_status_e status) {
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

char* TRI_GetCollectionNameByIdVocBase(TRI_vocbase_t* vocbase,
                                       TRI_voc_cid_t id) {
  READ_LOCKER(readLocker, vocbase->_collectionsLock);

  TRI_vocbase_col_t* found = static_cast<TRI_vocbase_col_t*>(
      TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));

  if (found == nullptr) {
    return nullptr;
  }

  std::string const colName(found->name());
  return TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, colName.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByNameVocBase(TRI_vocbase_t* vocbase,
                                                     char const* name) {
  char const c = *name;

  // if collection name is passed as a stringified id, we'll use the lookupbyid
  // function
  // this is safe because collection names must not start with a digit
  if (c >= '0' && c <= '9') {
    return TRI_LookupCollectionByIdVocBase(vocbase, TRI_UInt64String(name));
  }

  // otherwise we'll look up the collection by name
  READ_LOCKER(readLocker, vocbase->_collectionsLock);
  return static_cast<TRI_vocbase_col_t*>(
      TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a (document) collection by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_LookupCollectionByIdVocBase(TRI_vocbase_t* vocbase,
                                                   TRI_voc_cid_t id) {
  READ_LOCKER(readLocker, vocbase->_collectionsLock);
  return static_cast<TRI_vocbase_col_t*>(
      TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a collection by name, optionally creates it
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_FindCollectionByNameOrCreateVocBase(
    TRI_vocbase_t* vocbase, char const* name, const TRI_col_type_t type) {
  if (name == nullptr) {
    return nullptr;
  }

  TRI_vocbase_col_t* found = nullptr;

  {
    READ_LOCKER(readLocker, vocbase->_collectionsLock);

    if (name[0] >= '0' && name[0] <= '9') {
      // support lookup by id, too
      try {
        TRI_voc_cid_t id =
            arangodb::basics::StringUtils::uint64(name, strlen(name));
        found = static_cast<TRI_vocbase_col_t*>(
            TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &id));
      } catch (...) {
        // no need to throw here... found will still be a nullptr
      }
    } else {
      found = static_cast<TRI_vocbase_col_t*>(TRI_LookupByKeyAssociativePointer(
          &vocbase->_collectionsByName, name));
    }
  }

  if (found != nullptr) {
    return found;
  } else {
    // collection not found. now create it
    VPackBuilder builder;  // DO NOT FILL IT
    arangodb::VocbaseCollectionInfo parameter(
        vocbase, name, (TRI_col_type_e)type,
        (TRI_voc_size_t)vocbase->_settings.defaultMaximalSize, builder.slice());
    TRI_vocbase_col_t* collection =
        TRI_CreateCollectionVocBase(vocbase, parameter, 0, true);

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

TRI_vocbase_col_t* TRI_CreateCollectionVocBase(
    TRI_vocbase_t* vocbase, arangodb::VocbaseCollectionInfo& parameters,
    TRI_voc_cid_t cid, bool writeMarker) {
  // check that the name does not contain any strange characters
  if (!TRI_IsAllowedNameCollection(parameters.isSystem(),
                                   parameters.namec_str())) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);

    return nullptr;
  }

  READ_LOCKER(readLocker, vocbase->_inventoryLock);

  TRI_vocbase_col_t* collection;
  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    collection =
        CreateCollection(vocbase, parameters, cid, writeMarker, builder);
  }

  if (!writeMarker) {
    return collection;
  }

  if (collection == nullptr) {
    // something went wrong... must not continue
    return nullptr;
  }

  VPackSlice const slice = builder.slice();

  TRI_ASSERT(cid != 0);

  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::wal::CreateCollectionMarker marker(vocbase->_id, cid,
                                                 slice.toJson());
    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    return collection;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  LOG(WARN) << "could not save collection create marker in log: " << TRI_errno_string(res);

  // TODO: what to do here?
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a (document) collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnloadCollectionVocBase(TRI_vocbase_t* vocbase,
                                TRI_vocbase_col_t* collection, bool force) {
  if (!collection->_canUnload && !force) {
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

      TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
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
  collection->_collection->ditches()->createUnloadCollectionDitch(
      collection->_collection, collection, UnloadCollectionCallback, __FILE__,
      __LINE__);

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

int TRI_DropCollectionVocBase(TRI_vocbase_t* vocbase,
                              TRI_vocbase_col_t* collection, bool writeMarker) {
  TRI_ASSERT(collection != nullptr);

  if (!collection->_canDrop &&
      !arangodb::wal::LogfileManager::instance()->isInRecovery()) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  while (true) {
    DropState state = DROP_EXIT;
    int res;
    {
      READ_LOCKER(readLocker, vocbase->_inventoryLock);

      res = DropCollection(vocbase, collection, writeMarker, state);
    }

    if (state == DROP_PERFORM) {
      if (arangodb::wal::LogfileManager::instance()->isInRecovery()) {
        DropCollectionCallback(nullptr, collection);
      } else {
        // add callback for dropping
        collection->_collection->ditches()->createDropCollectionDitch(
            collection->_collection, collection, DropCollectionCallback,
            __FILE__, __LINE__);

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

int TRI_RenameCollectionVocBase(TRI_vocbase_t* vocbase,
                                TRI_vocbase_col_t* collection,
                                char const* newName, bool doOverride,
                                bool writeMarker) {
  if (!collection->_canRename) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  // lock collection because we are going to copy its current name
  std::string oldName;
  {
    READ_LOCKER(readLocker, collection->_lock);
    oldName = collection->name();
  }

  // old name should be different

  // check if names are actually different
  if (oldName == std::string(newName)) {
    return TRI_ERROR_NO_ERROR;
  }

  if (!doOverride) {
    bool isSystem;
    isSystem = TRI_IsSystemNameCollection(oldName.c_str());

    if (isSystem && !TRI_IsSystemNameCollection(newName)) {
      // a system collection shall not be renamed to a non-system collection
      // name
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    } else if (!isSystem && TRI_IsSystemNameCollection(newName)) {
      // a non-system collection shall not be renamed to a system collection
      // name
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }

    if (!TRI_IsAllowedNameCollection(isSystem, newName)) {
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }
  }

  READ_LOCKER(readLocker, vocbase->_inventoryLock);

  TRI_EVENTUAL_WRITE_LOCK_STATUS_VOCBASE_COL(collection);

  int res = RenameCollection(vocbase, collection, oldName.c_str(), newName);

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (res == TRI_ERROR_NO_ERROR && writeMarker) {
    // now log the operation
    try {
      arangodb::wal::RenameCollectionMarker marker(
          vocbase->_id, collection->_cid, std::string(newName));
      arangodb::wal::SlotInfoCopy slotInfo =
          arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                      false);

      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
      }

      return TRI_ERROR_NO_ERROR;
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(WARN) << "could not save collection rename marker in log: " << TRI_errno_string(res);
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage, loading or manifesting it
////////////////////////////////////////////////////////////////////////////////

int TRI_UseCollectionVocBase(TRI_vocbase_t* vocbase,
                             TRI_vocbase_col_t* collection,
                             TRI_vocbase_col_status_e& status) {
  return LoadCollectionVocBase(vocbase, collection, status);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by id
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByIdVocBase(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t cid,
    TRI_vocbase_col_status_e& status) {
  // .............................................................................
  // check that we have an existing name
  // .............................................................................

  TRI_vocbase_col_t const* collection = nullptr;
  {
    READ_LOCKER(readLocker, vocbase->_collectionsLock);
    collection = static_cast<TRI_vocbase_col_t const*>(
        TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsById, &cid));
  }

  if (collection == nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return nullptr;
  }

  // .............................................................................
  // try to load the collection
  // .............................................................................

  int res = LoadCollectionVocBase(
      vocbase, const_cast<TRI_vocbase_col_t*>(collection), status);

  if (res == TRI_ERROR_NO_ERROR) {
    return const_cast<TRI_vocbase_col_t*>(collection);
  }

  TRI_set_errno(res);

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a (document) collection for usage by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_UseCollectionByNameVocBase(
    TRI_vocbase_t* vocbase, char const* name,
    TRI_vocbase_col_status_e& status) {
  // .............................................................................
  // check that we have an existing name
  // .............................................................................

  TRI_vocbase_col_t const* collection = nullptr;

  {
    READ_LOCKER(readLocker, vocbase->_collectionsLock);
    collection = static_cast<TRI_vocbase_col_t const*>(
        TRI_LookupByKeyAssociativePointer(&vocbase->_collectionsByName, name));
  }

  if (collection == nullptr) {
    LOG(DEBUG) << "unknown collection '" << name << "'";

    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return nullptr;
  }

  // .............................................................................
  // try to load the collection
  // .............................................................................

  int res = LoadCollectionVocBase(
      vocbase, const_cast<TRI_vocbase_col_t*>(collection), status);

  if (res == TRI_ERROR_NO_ERROR) {
    return const_cast<TRI_vocbase_col_t*>(collection);
  }

  TRI_set_errno(res);
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a (document) collection from usage
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseCollectionVocBase(TRI_vocbase_t* vocbase,
                                  TRI_vocbase_col_t* collection) {
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the vocbase has been marked as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDeletedVocBase(TRI_vocbase_t* vocbase) {
  auto refCount = vocbase->_refCount.load();
  // if the stored value is odd, it means the database has been marked as
  // deleted
  return (refCount % 2 == 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the reference counter for a database
////////////////////////////////////////////////////////////////////////////////

bool TRI_UseVocBase(TRI_vocbase_t* vocbase) {
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

void TRI_ReleaseVocBase(TRI_vocbase_t* vocbase) {
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

bool TRI_DropVocBase(TRI_vocbase_t* vocbase) {
  auto oldValue = vocbase->_refCount.fetch_or(1, std::memory_order_release);
  // if the previously stored value is odd, it means the database has already
  // been marked as deleted
  return (oldValue % 2 == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database can be removed
////////////////////////////////////////////////////////////////////////////////

bool TRI_CanRemoveVocBase(TRI_vocbase_t* vocbase) {
  auto refCount = vocbase->_refCount.load();
  // we are intentionally comparing with exactly 1 here, because a 1 means
  // that noone else references the database but it has been marked as deleted
  return (refCount == 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the database is the system database
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemVocBase(TRI_vocbase_t* vocbase) {
  return TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a database name is allowed
///
/// Returns true if the name is allowed and false otherwise
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedNameVocBase(bool allowSystem, char const* name) {
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is
  // allowed
  for (char const* ptr = name; *ptr; ++ptr) {
    bool ok;
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') ||
             ('A' <= *ptr && *ptr <= 'Z');
      } else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    } else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') ||
           ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (!ok) {
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

TRI_voc_tick_t TRI_NextQueryIdVocBase(TRI_vocbase_t* vocbase) {
  return QueryId.fetch_add(1, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the "throw collection not loaded error"
////////////////////////////////////////////////////////////////////////////////

bool TRI_GetThrowCollectionNotLoadedVocBase(TRI_vocbase_t* vocbase) {
  return ThrowCollectionNotLoaded.load(std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the "throw collection not loaded error"
////////////////////////////////////////////////////////////////////////////////

void TRI_SetThrowCollectionNotLoadedVocBase(TRI_vocbase_t* vocbase,
                                            bool value) {
  ThrowCollectionNotLoaded.store(value, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vocbase object
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t::TRI_vocbase_t(TRI_server_t* server, TRI_vocbase_type_e type,
                             char const* path, TRI_voc_tick_t id,
                             char const* name,
                             TRI_vocbase_defaults_t const* defaults)
    : _id(id),
      _path(nullptr),
      _name(nullptr),
      _type(type),
      _refCount(0),
      _server(server),
      _deadlockDetector(false),
      _userStructures(nullptr),
      _queries(nullptr),
      _cursorRepository(nullptr),
      _collectionKeys(nullptr),
      _authInfoLoaded(false),
      _hasCompactor(false),
      _isOwnAppsDirectory(true),
      _oldTransactions(nullptr),
      _replicationApplier(nullptr) {
  _queries = new arangodb::aql::QueryList(this);
  _cursorRepository = new arangodb::CursorRepository(this);
  _collectionKeys = new arangodb::CollectionKeysRepository();

  _path = TRI_DuplicateString(TRI_CORE_MEM_ZONE, path);
  _name = TRI_DuplicateString(TRI_CORE_MEM_ZONE, name);

  // use the defaults provided
  defaults->applyToVocBase(this);

  // init collections
  _collections.reserve(32);
  _deadCollections.reserve(32);

  TRI_InitAssociativePointer(&_collectionsById, TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCid, HashElementCid, EqualKeyCid, nullptr);

  TRI_InitAssociativePointer(&_collectionsByName, TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCollectionName, HashElementCollectionName,
                             EqualKeyCollectionName, nullptr);

  TRI_InitAuthInfo(this);

  TRI_CreateUserStructuresVocBase(this);

  TRI_InitCondition(&_compactorCondition);
  TRI_InitCondition(&_cleanupCondition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a vocbase object
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t::~TRI_vocbase_t() {
  if (_userStructures != nullptr) {
    TRI_FreeUserStructuresVocBase(this);
  }

  // free replication
  if (_replicationApplier != nullptr) {
    delete _replicationApplier;
  }

  delete _oldTransactions;

  TRI_DestroyCondition(&_cleanupCondition);
  TRI_DestroyCondition(&_compactorCondition);

  TRI_DestroyAuthInfo(this);

  TRI_DestroyAssociativePointer(&_collectionsByName);
  TRI_DestroyAssociativePointer(&_collectionsById);

  delete _cursorRepository;
  delete _collectionKeys;
  delete _queries;

  // free name and path
  if (_path != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, _path);
  }
  if (_name != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, _name);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief note the progress of a connected replication client
////////////////////////////////////////////////////////////////////////////////

void TRI_vocbase_t::updateReplicationClient(TRI_server_id_t serverId,
                                            TRI_voc_tick_t lastFetchedTick) {
  WRITE_LOCKER(writeLocker, _replicationClientsLock);

  try {
    auto it = _replicationClients.find(serverId);

    if (it == _replicationClients.end()) {
      _replicationClients.emplace(
          serverId, std::make_pair(TRI_microtime(), lastFetchedTick));
    } else {
      (*it).second.first = TRI_microtime();
      if (lastFetchedTick > 0) {
        (*it).second.second = lastFetchedTick;
      }
    }
  } catch (...) {
    // silently fail...
    // all we would be missing is the progress information of a slave
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the progress of all replication clients
////////////////////////////////////////////////////////////////////////////////

std::vector<std::tuple<TRI_server_id_t, double, TRI_voc_tick_t>>
TRI_vocbase_t::getReplicationClients() {
  std::vector<std::tuple<TRI_server_id_t, double, TRI_voc_tick_t>> result;

  READ_LOCKER(readLocker, _replicationClientsLock);

  for (auto& it : _replicationClients) {
    result.emplace_back(
        std::make_tuple(it.first, it.second.first, it.second.second));
  }
  return result;
}
