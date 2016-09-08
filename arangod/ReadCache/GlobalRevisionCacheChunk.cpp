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

#include "GlobalRevisionCacheChunk.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "ReadCache/RevisionTypes.h"

#include "Logger/Logger.h"

using namespace arangodb;

// align the length value to a multiple of 8
static constexpr inline uint32_t AlignSize(uint32_t value) {
  return (value + 7) - ((value + 7) & 7);
}
  
// create a new chunk of the specified size
GlobalRevisionCacheChunk::GlobalRevisionCacheChunk(uint32_t size) 
    : _memory(nullptr), _currentUsers(0), _references(0), _version(0), _writePosition(0), _size(size) {
  // if this throws, no harm is done
  _memory = new char[size];
  //LOG(ERR) << "CREATING CHUNK OF SIZE " << _size << ", PTR: " << (void*) _memory;
}

GlobalRevisionCacheChunk::~GlobalRevisionCacheChunk() {
  //LOG(ERR) << "DESTROYING CHUNK OF SIZE " << _size << ", PTR: " << (void*) _memory;
  delete[] _memory;
}

// stores a revision in the cache, acquiring a lease
// the collection id is prepended to the actual data in order to quickly access
// the shard-local hash for the revision when cleaning up the chunk 
RevisionReader GlobalRevisionCacheChunk::storeAndLease(uint64_t collectionId, uint8_t const* data, size_t length) {
  //LOG(ERR) << "STORING AND LEASING ELEMENT OF LENGTH: " << length;
  if (!addReader()) {
    // chunk is being garbage-collected at the moment
    THROW_ARANGO_EXCEPTION(TRI_ERROR_LOCKED);
  }

  try {
    uint32_t const version = _version.load(std::memory_order_relaxed);
    uint32_t const offset = store(collectionId, data, length);

    addReference();
  
    return RevisionReader(this, RevisionOffset(offset), RevisionVersion(version)); 
  } catch (...) {
    // decrease the reference counter in case we cannot store the data in the chunk
    removeReader();
    throw;
  }
}

// stores a revision in the cache, without acquiring a lease
// the collection id is prepended to the actual data in order to quickly access
// the shard-local hash for the revision when cleaning up the chunk 
uint32_t GlobalRevisionCacheChunk::store(uint64_t collectionId, uint8_t const* data, size_t length) {
  uint32_t const offset = adjustWritePosition(static_cast<uint32_t>(physicalSize(length)));

  // we can copy the data into the chunk without the lock
  storeAtOffset(offset, collectionId, data, length);
  return offset;
}

// return the physical size for a piece of data
// this adds required padding plus the required size for the collection id
size_t GlobalRevisionCacheChunk::physicalSize(size_t dataLength) noexcept {
  return AlignSize(static_cast<uint32_t>(sizeof(uint64_t) + dataLength));
}
  
// garbage collects a chunk
// this will prepare the chunk for reuse, but not free the chunk's underlying memory
void GlobalRevisionCacheChunk::garbageCollect(GarbageCollectionCallback const& callback) {
  // invalidates the chunk by simply increasing the version number
  // this will make subsequent client read requests fail
  ++_version;

  // now add ourselves as an exclusive writer
  addWriter();

  TRI_DEFER(removeWriter());

  MUTEX_LOCKER(locker, _writeLock);
  char const* ptr = _memory;
  char const* end = ptr + _writePosition;
  while (ptr < end) {
    uint64_t collectionId = *reinterpret_cast<uint64_t const*>(ptr);
    arangodb::velocypack::Slice slice(ptr + sizeof(uint64_t));

    callback(collectionId, slice);
    ptr += AlignSize(static_cast<uint32_t>(sizeof(uint64_t) + slice.byteSize()));
  }

  // done collecting. now reset the cache and reset the write position
  memset(_memory, 0, _writePosition); 
  _writePosition = 0;
}

// add a reader for the chunk, making it uneligible for garbage collection
bool GlobalRevisionCacheChunk::addReader() noexcept {
  //LOG(ERR) << "ADDING READER. VALUE IS " << _currentUsers.load();
  // increase the reference count value by 2. we use the lowest bit of the
  // reference count to indicate that the chunk is going to be garbage-collected.
  uint32_t oldValue = _currentUsers.fetch_add(2, std::memory_order_release);

  //LOG(ERR) << "ADDING READER. OLD VALUE IS " << oldValue;

  if ((oldValue & 1UL) == 0UL) {
    // garbage collection bit not set. now we've successfully added the reader
  //LOG(ERR) << "GC BIT NOT SET IN OLDVALUE";
    return true;
  }

  //LOG(ERR) << "GC BIT IS SET IN OLDVALUE!!";
  // garbage collection bit was set. revert the operation and return failure
  removeReader();
  return false;
}

// remove a reader for the chunk, making it eligible for garbage collection
void GlobalRevisionCacheChunk::removeReader() noexcept {
  //LOG(ERR) << "REMOVING READER";
  _currentUsers -= 2;
}
 
// modifies the reference count so that after this method call there is only
// a single writer (ourselves), and all readers are blocked 
void GlobalRevisionCacheChunk::addWriter() noexcept {
  // wait until there are no more readers active
  //LOG(ERR) << "ADDING WRITER!";
  uint32_t expected = 0UL;
  while (!_currentUsers.compare_exchange_weak(expected, 1UL, std::memory_order_release, std::memory_order_relaxed)) {
    expected = 0UL;
  }
} 

// modifies the reference count so that the exclusive writer bit is removed
// from the reference count
void GlobalRevisionCacheChunk::removeWriter() noexcept {
  //LOG(ERR) << "REMOVING WRITER!";
  // wait until there are no more readers active
  uint32_t expected = 1UL;
  while (!_currentUsers.compare_exchange_weak(expected, 0UL, std::memory_order_release, std::memory_order_relaxed)) {
    expected = 1UL;
  }
}
  
// add an external reference to the chunk. the chunk cannot be deleted physically
// if the number of external references is greater than 0
void GlobalRevisionCacheChunk::addReference() noexcept {
  ++_references;
}
  
// remove an external reference to the chunk. the chunk cannot be deleted physically
// if the number of external references is greater than 0
void GlobalRevisionCacheChunk::removeReference() noexcept {
  --_references;
}

// stores the byte range [data...data+length) at the specified offset in the chunk
// the data is prepended by the collection id passed
void GlobalRevisionCacheChunk::storeAtOffset(uint32_t offset, uint64_t collectionId, uint8_t const* data, size_t length) noexcept {
  // offset should always be evenly divisible by 8
  TRI_ASSERT(offset % 8 == 0);

  // we can copy the data into the chunk without the lock

  // copy collection id into chunk
  memcpy(_memory + offset, &collectionId, sizeof(collectionId));
  // copy data into chunk
  memcpy(_memory + offset + sizeof(collectionId), data, length);
}

// moves the write position of the chunk forward
uint32_t GlobalRevisionCacheChunk::adjustWritePosition(uint32_t length) {
  //LOG(ERR) << "ADJUSTWRITEPOSITION FOR LENGTH: " << length;
  // atomically check and move write pointer
  MUTEX_LOCKER(locker, _writeLock);

  if (_size - _writePosition < length) {
    // chunk is full
    //LOG(ERR) << "CHUNK IS FULL";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  //LOG(ERR) << "MOVING WRITE POSITION FROM " << _writePosition;
  uint32_t offset = _writePosition;
  _writePosition += length;
  return offset;
}

