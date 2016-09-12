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

#ifndef ARANGOD_READ_CACHE_COLLECTION_REVISION_CACHE_H
#define ARANGOD_READ_CACHE_COLLECTION_REVISION_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "ReadCache/RevisionReader.h"
#include "ReadCache/RevisionTypes.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class GlobalRevisionCache;

namespace wal {
class LogfileManager;
}

class GlobalRevisionCacheChunk;

// the global revision cache for documents
class CollectionRevisionCache {
 public:
  CollectionRevisionCache() = delete;
  CollectionRevisionCache(CollectionRevisionCache const&) = delete;
  CollectionRevisionCache& operator=(CollectionRevisionCache const&) = delete;

  // create the cache instance
  CollectionRevisionCache(TRI_voc_cid_t collectionId,
                          GlobalRevisionCache* globalCache, 
                          arangodb::wal::LogfileManager* logfileManager);

  // destroy the cache instance
  ~CollectionRevisionCache();

 public:
  // insert a revision into the shard-local cache from the WAL 
  void insertFromWal(uint64_t revisionId, TRI_voc_fid_t datafileId, uint32_t offset);

  // insert a revision into the shard-local cache from the revision cache
  void insertFromRevisionCache(uint64_t revisionId, RevisionReader* reader);
  
  // remove a revision from the shard-local cache 
  void remove(uint64_t revisionId);
  
  // read a revision from the shard-local cache. the revision can safely be accessed
  // while the RevisionReader is in place
  RevisionReader lookup(uint64_t revisionId);

 private:
  // collection id
  //TRI_voc_cid_t const                            _collectionId;

  // global document revision cache
  //GlobalRevisionCache*                           _globalCache;

  // WAL logfile manager, used for reference counting of WAL files
  //arangodb::wal::LogfileManager*                 _logfileManager;
  
  // note: _revisionsLock and _revisions should be physically partitioned 
  // in order to avoid lock contention

  // lock for the revision hash table
  arangodb::basics::ReadWriteLock                _revisionsLock;
  
  // hash table for revisions
  std::unordered_map<uint64_t, RevisionLocation> _revisions;
};

}

#endif
