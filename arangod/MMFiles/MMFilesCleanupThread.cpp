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

#include "MMFilesCleanupThread.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesDitch.h"
#include "MMFiles/MMFilesEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/CursorRepository.h"
#include "VocBase/LogicalCollection.h"
#include "MMFiles/MMFilesLogfileManager.h"

using namespace arangodb;
  
MMFilesCleanupThread::MMFilesCleanupThread(TRI_vocbase_t* vocbase) 
    : Thread("MMFilesCleanup"), _vocbase(vocbase) {}

MMFilesCleanupThread::~MMFilesCleanupThread() { shutdown(); }

void MMFilesCleanupThread::signal() {
  CONDITION_LOCKER(locker, _condition);
  locker.signal();
}

/// @brief cleanup event loop
void MMFilesCleanupThread::run() {
  MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
  uint64_t iterations = 0;

  std::vector<arangodb::LogicalCollection*> collections;

  while (true) {
    // keep initial _state value as vocbase->_state might change during cleanup
    // loop
    TRI_vocbase_t::State state = _vocbase->state();

    ++iterations;

    try {

      if (state == TRI_vocbase_t::State::SHUTDOWN_COMPACTOR ||
          state == TRI_vocbase_t::State::SHUTDOWN_CLEANUP) {
        // cursors must be cleaned before collections are handled
        // otherwise the cursors may still hold barriers on collections
        // and collections cannot be closed properly
        cleanupCursors(true);
      }
        
      // check if we can get the compactor lock exclusively
      // check if compaction is currently disallowed
      engine->tryPreventCompaction(_vocbase, [this, &collections, &iterations](TRI_vocbase_t* vocbase) {
        try {
          // copy all collections
          collections = vocbase->collections(true);
        } catch (...) {
          collections.clear();
        }

        for (auto& collection : collections) {
          TRI_ASSERT(collection != nullptr);

          TRI_vocbase_col_status_e status = collection->getStatusLocked();

          if (status != TRI_VOC_COL_STATUS_LOADED && 
              status != TRI_VOC_COL_STATUS_UNLOADING &&
              status != TRI_VOC_COL_STATUS_DELETED) {
            continue;
          }
            
          // we're the only ones that can unload the collection, so using
          // the collection pointer outside the lock is ok
          cleanupCollection(collection);
        }
      }, false);

      // server is still running, clean up unused cursors
      if (iterations % cleanupCursorIterations() == 0) {
        cleanupCursors(false);

        // clean up expired compactor locks
        engine->cleanupCompactionBlockers(_vocbase);
      }

      if (state == TRI_vocbase_t::State::NORMAL) {
        CONDITION_LOCKER(locker, _condition);
        locker.wait(cleanupInterval());
      } else {
        // prevent busy waiting
        usleep(10000);
      }

    } catch (...) {
      // simply ignore the errors here
    }

    if (state == TRI_vocbase_t::State::SHUTDOWN_CLEANUP || isStopping()) {
      // server shutdown
      break;
    }
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "shutting down cleanup thread";
}

/// @brief clean up cursors
void MMFilesCleanupThread::cleanupCursors(bool force) {
  // clean unused cursors
  auto cursors = _vocbase->cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  try {
    cursors->garbageCollect(force);
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught exception during cursor cleanup";
  }
}

/// @brief checks all datafiles of a collection
void MMFilesCleanupThread::cleanupCollection(arangodb::LogicalCollection* collection) {
  // unload operations can normally only be executed when a collection is fully
  // garbage collected
  bool unloadChecked = false;

  // but if we are in server shutdown, we can force unloading of collections
  bool isInShutdown = application_features::ApplicationServer::isStopping();

  // loop until done
    
  auto mmfiles = arangodb::MMFilesCollection::toMMFilesCollection(collection);
  TRI_ASSERT(mmfiles != nullptr);
    
  while (true) {
    auto ditches = mmfiles->ditches();

    TRI_ASSERT(ditches != nullptr);

    // check and remove all callback elements at the beginning of the list
    auto callback = [&](arangodb::MMFilesDitch const* ditch) -> bool {
      if (ditch->type() == arangodb::MMFilesDitch::TRI_DITCH_COLLECTION_UNLOAD) {
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
      TRI_vocbase_col_status_e s = collection->tryFetchStatus(gotStatus);
      if (gotStatus) {
        isUnloading = (s == TRI_VOC_COL_STATUS_UNLOADING);
      }

      if (gotStatus && !isUnloading) {
        popped = false;
        auto unloader =
            ditches->process(popped, [](arangodb::MMFilesDitch const* ditch) -> bool {
              return (ditch->type() ==
                      arangodb::MMFilesDitch::TRI_DITCH_COLLECTION_UNLOAD);
            });
        if (popped) {
          // we've changed the list. try with current state in next turn
          TRI_ASSERT(unloader != nullptr);
          delete unloader;
          return;
        }
      }
      
      MMFilesCollection* mmColl = MMFilesCollection::toMMFilesCollection(collection->getPhysical());
      if (!mmColl->isFullyCollected()) {
        bool isDeleted = false;

        // if there is still some garbage collection to perform,
        // check if the collection was deleted already
        bool found;
        TRI_vocbase_col_status_e s = collection->tryFetchStatus(found);
        if (found) {
          isDeleted = (s == TRI_VOC_COL_STATUS_DELETED);
        }

        if (!isDeleted && collection->vocbase()->isDropped()) {
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

    // execute callback, some of the callbacks might delete or unload our collection
    auto const type = ditch->type();
  
    if (type == arangodb::MMFilesDitch::TRI_DITCH_DATAFILE_DROP) {
      static_cast<arangodb::MMFilesDropDatafileDitch*>(ditch)->executeCallback();
      delete ditch;
      // next iteration
    } else if (type == arangodb::MMFilesDitch::TRI_DITCH_DATAFILE_RENAME) {
      static_cast<arangodb::MMFilesRenameDatafileDitch*>(ditch)->executeCallback();
      delete ditch;
      // next iteration
    } else if (type == arangodb::MMFilesDitch::TRI_DITCH_COLLECTION_UNLOAD) {
      // collection will be unloaded
      bool hasUnloaded = static_cast<arangodb::MMFilesUnloadCollectionDitch*>(ditch)
                             ->executeCallback();
      delete ditch;

      if (hasUnloaded) {
        // this has unloaded and freed the collection
        return;
      }
    } else if (type == arangodb::MMFilesDitch::TRI_DITCH_COLLECTION_DROP) {
      // collection will be dropped
      bool hasDropped = static_cast<arangodb::MMFilesDropCollectionDitch*>(ditch)
                            ->executeCallback();
      delete ditch;

      if (hasDropped) {
        // this has dropped the collection
        return;
      }
    } else {
      // unknown type
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "unknown ditch type '" << type << "'"; FATAL_ERROR_EXIT();
    }

    // next iteration
  }
}
