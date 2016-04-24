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

#include "cleanup.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ReadLocker.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Utils/CursorRepository.h"
#include "VocBase/Ditch.h"
#include "VocBase/compactor.h"
#include "VocBase/document-collection.h"
#include "Wal/LogfileManager.h"

using namespace arangodb::application_features;

////////////////////////////////////////////////////////////////////////////////
/// @brief clean interval in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const CLEANUP_INTERVAL = (1 * 1000 * 1000);

////////////////////////////////////////////////////////////////////////////////
/// @brief how many cleanup iterations until shadows are cleaned
////////////////////////////////////////////////////////////////////////////////

static int const CLEANUP_CURSOR_ITERATIONS = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief how many cleanup iterations until indexes are cleaned
////////////////////////////////////////////////////////////////////////////////

static int const CLEANUP_INDEX_ITERATIONS = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static void CleanupDocumentCollection(TRI_vocbase_col_t* collection,
                                      TRI_document_collection_t* document) {
  // unload operations can normally only be executed when a collection is fully
  // garbage collected
  bool unloadChecked = false;

  // but if we are in server shutdown, we can force unloading of collections
  bool isInShutdown = ApplicationServer::isStopping();

  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(document != nullptr);

  // loop until done
  while (true) {
    auto ditches = document->ditches();

    // check and remove all callback elements at the beginning of the list
    auto callback = [&](arangodb::Ditch const* ditch) -> bool {
      if (ditch->type() == arangodb::Ditch::TRI_DITCH_COLLECTION_UNLOAD) {
        // check if we can really unload, this is only the case if the
        // collection's WAL markers
        // were fully collected

        if (!unloadChecked && !isInShutdown) {
          return false;
        }
        // fall-through intentional
      } else {
        // retry in next iteration
        unloadChecked = false;
      }

      return true;
    };

    bool popped = false;
    auto ditch = ditches->process(popped, callback);

    if (ditch == nullptr) {
      // absolutely nothing to do
      return;
    }

    TRI_ASSERT(ditch != nullptr);

    if (!popped) {
      // we'll be getting here only if an UNLOAD ditch is at the head of the
      // list

      // check if the collection is still in the "unloading" state. if not,
      // then someone has already triggered a reload or a deletion of the
      // collection
      bool isUnloading = false;
      bool gotStatus = false;
      {
        TRY_READ_LOCKER(readLocker, collection->_lock);

        if (readLocker.isLocked()) {
          isUnloading = (collection->_status == TRI_VOC_COL_STATUS_UNLOADING);
          gotStatus = true;
        }
      }

      if (gotStatus && !isUnloading) {
        popped = false;
        auto unloader =
            ditches->process(popped, [](arangodb::Ditch const* ditch) -> bool {
              return (ditch->type() ==
                      arangodb::Ditch::TRI_DITCH_COLLECTION_UNLOAD);
            });
        if (popped) {
          // we've changed the list. try with current state in next turn
          TRI_ASSERT(unloader != nullptr);
          delete unloader;
          return;
        }
      }

      if (!document->isFullyCollected()) {
        bool isDeleted = false;

        // if there is still some garbage collection to perform,
        // check if the collection was deleted already
        {
          TRY_READ_LOCKER(readLocker, collection->_lock);

          if (readLocker.isLocked()) {
            isDeleted = (collection->_status == TRI_VOC_COL_STATUS_DELETED);
          }
        }

        if (!isDeleted && TRI_IsDeletedVocBase(collection->_vocbase)) {
          // the collection was not marked as deleted, but the database was
          isDeleted = true;
        }

        if (!isDeleted) {
          // collection is not fully collected and still undeleted - postpone
          // the unload
          return;
        }
        // if deleted, then we may unload / delete
      }

      unloadChecked = true;
      continue;
    }

    // if we got here, the ditch was already unlinked from the list of ditches
    // if we free it, we therefore must not use the freeDitch method!

    // someone else might now insert a new TRI_DITCH_DOCUMENT now, but it will
    // always refer to a different datafile than the one that we will now unload

    // execute callback, sone of the callbacks might delete or free our
    // collection
    auto const type = ditch->type();

    if (type == arangodb::Ditch::TRI_DITCH_DATAFILE_DROP) {
      dynamic_cast<arangodb::DropDatafileDitch*>(ditch)->executeCallback();
      delete ditch;
      // next iteration
    } else if (type == arangodb::Ditch::TRI_DITCH_DATAFILE_RENAME) {
      dynamic_cast<arangodb::RenameDatafileDitch*>(ditch)->executeCallback();
      delete ditch;
      // next iteration
    } else if (type == arangodb::Ditch::TRI_DITCH_COLLECTION_UNLOAD) {
      // collection will be unloaded
      bool hasUnloaded = dynamic_cast<arangodb::UnloadCollectionDitch*>(ditch)
                             ->executeCallback();
      delete ditch;

      if (hasUnloaded) {
        // this has unloaded and freed the collection
        return;
      }
    } else if (type == arangodb::Ditch::TRI_DITCH_COLLECTION_DROP) {
      // collection will be dropped
      bool hasDropped = dynamic_cast<arangodb::DropCollectionDitch*>(ditch)
                            ->executeCallback();
      delete ditch;

      if (hasDropped) {
        // this has dropped the collection
        return;
      }
    } else {
      // unknown type
      LOG(FATAL) << "unknown ditch type '" << type << "'"; FATAL_ERROR_EXIT();
    }

    // next iteration
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clean up cursors
////////////////////////////////////////////////////////////////////////////////

static void CleanupCursors(TRI_vocbase_t* vocbase, bool force) {
  // clean unused cursors
  auto cursors =
      static_cast<arangodb::CursorRepository*>(vocbase->_cursorRepository);
  TRI_ASSERT(cursors != nullptr);

  try {
    cursors->garbageCollect(force);
  } catch (...) {
    LOG(WARN) << "caught exception during cursor cleanup";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupVocBase(void* data) {
  uint64_t iterations = 0;

  TRI_vocbase_t* const vocbase = static_cast<TRI_vocbase_t*>(data);
  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(vocbase->_state == 1);

  std::vector<TRI_vocbase_col_t*> collections;

  while (true) {
    // keep initial _state value as vocbase->_state might change during cleanup
    // loop
    int state = vocbase->_state;

    ++iterations;

    if (state == (sig_atomic_t)TRI_VOCBASE_STATE_SHUTDOWN_COMPACTOR ||
        state == (sig_atomic_t)TRI_VOCBASE_STATE_SHUTDOWN_CLEANUP) {
      // shadows must be cleaned before collections are handled
      // otherwise the shadows might still hold barriers on collections
      // and collections cannot be closed properly
      CleanupCursors(vocbase, true);
    }

    // check if we can get the compactor lock exclusively
    if (TRI_CheckAndLockCompactorVocBase(vocbase)) {
      try {
        READ_LOCKER(readLocker, vocbase->_collectionsLock);
        // copy all collections
        collections = vocbase->_collections;
      } catch (...) {
        collections.clear();
      }

      for (auto& collection : collections) {
        TRI_ASSERT(collection != nullptr);
        TRI_document_collection_t* document;

        {
          READ_LOCKER(readLocker, collection->_lock);
          document = collection->_collection;
        }

        if (document == nullptr) {
          // collection currently not loaded
          continue;
        }

        TRI_ASSERT(document != nullptr);

        // we're the only ones that can unload the collection, so using
        // the collection pointer outside the lock is ok

        // maybe cleanup indexes, unload the collection or some datafiles
        // clean indexes?
        if (iterations % (uint64_t)CLEANUP_INDEX_ITERATIONS == 0) {
          document->cleanupIndexes(document);
        }

        CleanupDocumentCollection(collection, document);
      }

      TRI_UnlockCompactorVocBase(vocbase);
    }

    if (vocbase->_state >= 1) {
      // server is still running, clean up unused cursors
      if (iterations % CLEANUP_CURSOR_ITERATIONS == 0) {
        CleanupCursors(vocbase, false);

        // clean up expired compactor locks
        TRI_CleanupCompactorVocBase(vocbase);
      }

      if (state == 1) {
        TRI_LockCondition(&vocbase->_cleanupCondition);
        TRI_TimedWaitCondition(&vocbase->_cleanupCondition,
                               (uint64_t)CLEANUP_INTERVAL);
        TRI_UnlockCondition(&vocbase->_cleanupCondition);
      } else if (state > 1) {
        // prevent busy waiting
        usleep(10000);
      }
    }

    if (state == 3) {
      // server shutdown
      break;
    }
  }

  LOG(TRACE) << "shutting down cleanup thread";
}
