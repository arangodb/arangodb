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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CollectionRevisionCache.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "ReadCache/GlobalRevisionCacheChunk.h"
#include "Wal/LogfileManager.h"
    
#include "Logger/Logger.h"

using namespace arangodb;

// a collection-specific cache for documents
CollectionRevisionCache::CollectionRevisionCache(TRI_voc_cid_t collectionId,
                                                 GlobalRevisionCache* globalCache,
                                                 arangodb::wal::LogfileManager* logfileManager) {}
                                                 /*
    : _collectionId(collectionId),
      _globalCache(globalCache),
      _logfileManager(logfileManager) {
}
*/

CollectionRevisionCache::~CollectionRevisionCache() {
  // TODO: decrease ref counts in global cache
}
  
// insert a revision into the shard-local cache from the WAL
void CollectionRevisionCache::insertFromWal(uint64_t revisionId, TRI_voc_fid_t datafileId, uint32_t offset) {
  // TODO: _logfileManager->addReference(datafileId);

  try {
    RevisionLocation location(datafileId, offset);

    WRITE_LOCKER(locker, _revisionsLock);

    auto it = _revisions.emplace(revisionId, location);

    if (!it.second) {
      // insertion unsuccessful because there is already a previous value. 
      // now look up the previous value and handle it
      RevisionLocation& previous = (*(it.first)).second;
      if (previous.isInWal()) {
        // previous entry points into a WAL file
        // TODO: _logfileManager->removeReference(previous.datafileId());
      } else {
        // previous entry points into the global revision cache
        previous.chunk()->removeReference();
      }

      // update value in place with new location
      previous = location;
    }
  } catch (...) {
    // roll back
    // TODO: _logfileManager->removeReference(datafileId);
    throw;
  }
}

// insert a revision into the shard-local cache from the revision cache
void CollectionRevisionCache::insertFromRevisionCache(uint64_t revisionId, RevisionReader* reader) {
  RevisionLocation location(reader->chunk(), reader->offset(), reader->version());

  WRITE_LOCKER(locker, _revisionsLock);

  auto it = _revisions.emplace(revisionId, location);

  if (!it.second) {
    // insertion unsuccessful because there is already a previous value. 
    // now look up the previous value and handle it
    RevisionLocation& previous = (*(it.first)).second;
    if (previous.isInWal()) {
      // previous entry points into a WAL file
      // TODO: implement this
      // _logfileManager->removeReference(previous.datafileId());
    } else {
      // previous entry points into the global revision cache
      previous.chunk()->removeReference();
    }

    // update value in place with new location
    previous = location;
    // now the reference to the chunk is stored in the _revision hash table
    // the reader is not responsible for freeing it later
    reader->stealReference();
  }
}
  
// remove a revision from the shard-local cache
void CollectionRevisionCache::remove(uint64_t revisionId) {
  WRITE_LOCKER(locker, _revisionsLock);

  auto it = _revisions.find(revisionId);

  if (it != _revisions.end()) {
    // previous element found
    RevisionLocation& previous = (*it).second;
    if (previous.isInWal()) {
      // previous entry points into a WAL file
      // TODO: implement this
      // _logfileManager->removeReference(previous.datafileId());
    } else {
      // previous entry points into the global revision cache
      previous.chunk()->removeReference();
    }

    _revisions.erase(it);
  }
}
  
// read a revision from the shard-local cache. the revision can safely be accessed
// while the RevisionReader is in place
RevisionReader CollectionRevisionCache::lookup(uint64_t revisionId) {
  READ_LOCKER(locker, _revisionsLock);

  auto it = _revisions.find(revisionId);

  if (it == _revisions.end()) {
    // not found
    return RevisionReader();
  }

  // found
  // TODO
  return RevisionReader();
}
  
