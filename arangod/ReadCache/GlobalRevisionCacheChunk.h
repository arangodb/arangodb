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

#ifndef ARANGOD_READ_CACHE_GLOBAL_REVISION_CACHE_CHUNK_H
#define ARANGOD_READ_CACHE_GLOBAL_REVISION_CACHE_CHUNK_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "ReadCache/RevisionReader.h"
#include "ReadCache/RevisionTypes.h"

#include <velocypack/Slice.h>

namespace arangodb {

// chunk managed by the revision cache
class GlobalRevisionCacheChunk {
 friend class RevisionReader;

 public:
  GlobalRevisionCacheChunk() = delete;
  GlobalRevisionCacheChunk(GlobalRevisionCacheChunk const&) = delete;
  GlobalRevisionCacheChunk& operator=(GlobalRevisionCacheChunk const&) = delete;

  // create a new chunk of the specified size
  explicit GlobalRevisionCacheChunk(uint32_t size);

  // destroy a chunk
  ~GlobalRevisionCacheChunk();

  // return the byte size of the chunk
  inline uint32_t size() const { return _size; }

  // return the number of active readers for this chunk
  inline bool hasReaders() const { return _currentUsers.load() != 0UL; }

  // return the number of external references for this chunk
  inline uint32_t hasReferences() const { return _references.load() != 0UL; }
  
  // stores a revision in the cache, acquiring a lease
  // the collection id is prepended to the actual data in order to quickly access
  // the shard-local hash for the revision when cleaning up the chunk 
  RevisionReader storeAndLease(uint64_t collectionId, uint8_t const* data, size_t length);
  
  // stores a revision in the cache, acquiring a lease
  RevisionReader storeAndLease(uint64_t collectionId, arangodb::velocypack::Slice const& data) {
    // simply forward to the pointer-based function
    return storeAndLease(collectionId, data.begin(), data.byteSize());
  }

  inline char const* read(RevisionOffset offset) const {
    return _memory + offset.value;
  }

  // stores a revision in the cache, without acquiring a lease
  // the collection id is prepended to the actual data in order to quickly access
  // the shard-local hash for the revision when cleaning up the chunk 
  uint32_t store(uint64_t collectionId, uint8_t const* data, size_t length);
  
  // stores a revision in the cache, without acquiring a lease
  uint32_t store(uint64_t collectionId, arangodb::velocypack::Slice const& data) {
    // simply forward to the pointer-based function
    return store(collectionId, data.begin(), data.byteSize());
  }

  // garbage collects a chunk
  // this will prepare the chunk for reuse, but not free the chunk's underlying memory
  void garbageCollect(GarbageCollectionCallback const& callback);
  
  // remove an external reference to the chunk. the chunk cannot be deleted physically
  // if the number of external references is greater than 0
  void removeReference() noexcept;

  // return the physical size for a piece of data
  // this adds required padding plus the required size for the collection id
  static size_t physicalSize(size_t dataLength) noexcept;

 private:
  // add a reader for the chunk, making it uneligible for garbage collection
  bool addReader() noexcept;

  // remove a reader for the chunk, making it eligible for garbage collection
  void removeReader() noexcept;

  // modifies the reference count so that after this method call there is only
  // a single writer (ourselves), and all readers are blocked 
  void addWriter() noexcept;

  // modifies the reference count so that the exclusive writer bit is removed
  // from the reference count
  void removeWriter() noexcept;
  
  // add an external reference to the chunk. the chunk cannot be deleted physically
  // if the number of external references is greater than 0
  void addReference() noexcept;
  
  // stores the byte range [data...data+length) at the specified offset in the chunk
  // the data is prepended by the collection id passed
  void storeAtOffset(uint32_t offset, uint64_t collectionId, uint8_t const* data, size_t length) noexcept;

  // moves the write position of the chunk forward
  uint32_t adjustWritePosition(uint32_t length);

 private:
  // pointer to the chunk's raw memory
  char*                  _memory;

  // number of users currently reading from/writing to this chunk
  std::atomic<uint32_t>  _currentUsers;
  
  // number of references handed out
  std::atomic<uint32_t>  _references;
  
  // version number for this chunk
  std::atomic<uint32_t>  _version;

  // mutex protecting writes to this chunk
  arangodb::Mutex        _writeLock;

  // current position for append-only writing to chunk memory
  uint32_t               _writePosition;

  // size of the chunk's memory
  uint32_t const         _size;
};

}

#endif
