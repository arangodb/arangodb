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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_INDEX_BUCKET_H
#define ARANGODB_BASICS_INDEX_BUCKET_H 1

#include "Basics/Common.h"
#include "Basics/files.h"
#include "Basics/memory-map.h"
#include "Logger/Logger.h"

namespace arangodb {
namespace basics {

template <class EntryType, class IndexType, size_t FileBackedThreshold>
struct IndexBucket {
  IndexType _nrAlloc;       // the size of the table
  IndexType _nrUsed;        // the number of used entries
  IndexType _nrCollisions;  // the number of entries that have
                            // a key that was previously in the table
  EntryType* _table;        // the table itself
  int _file;        // file descriptor for memory mapped file (-1 = no file)
  char* _filename;  // name of memory mapped file (nullptr = no file)
  void* _mmHandle;

  IndexBucket()
      : _nrAlloc(0),
        _nrUsed(0),
        _nrCollisions(0),
        _table(nullptr),
        _file(-1),
        _filename(nullptr),
        _mmHandle(nullptr) {}
  IndexBucket(IndexBucket const&) = delete;
  IndexBucket& operator=(IndexBucket const&) = delete;

  // move ctor. this takes over responsibility for the resources from other
  IndexBucket(IndexBucket&& other)
      : _nrAlloc(other._nrAlloc),
        _nrUsed(other._nrUsed),
        _nrCollisions(other._nrCollisions),
        _table(other._table),
        _file(other._file),
        _filename(other._filename),
        _mmHandle(other._mmHandle) {
    other._nrAlloc = 0;
    other._nrUsed = 0;
    other._nrCollisions = 0;
    other._table = nullptr;
    other._file = -1;
    other._filename = nullptr;
    other._mmHandle = nullptr;
  }

  IndexBucket& operator=(IndexBucket&& other) {
    deallocate();  // free own resources first

    _nrAlloc = other._nrAlloc;
    _nrUsed = other._nrUsed;
    _nrCollisions = other._nrCollisions;
    _table = other._table;
    _file = other._file;
    _filename = other._filename;
    _mmHandle = other._mmHandle;

    other._nrAlloc = 0;
    other._nrUsed = 0;
    other._nrCollisions = 0;
    other._table = nullptr;
    other._file = -1;
    other._filename = nullptr;
    other._mmHandle = nullptr;

    return *this;
  }

  ~IndexBucket() { deallocate(); }

 public:
  size_t memoryUsage() const { return requiredSize(_nrAlloc); }

  size_t requiredSize(size_t numberElements) const {
    return numberElements * sizeof(EntryType);
  }

  void allocate(size_t numberElements) {
    TRI_ASSERT(_nrAlloc == 0);
    TRI_ASSERT(_nrUsed == 0);
    TRI_ASSERT(_table == nullptr);
    TRI_ASSERT(_file == -1);
    TRI_ASSERT(_filename == nullptr);
    TRI_ASSERT(_mmHandle == nullptr);

    size_t const totalSize = requiredSize(numberElements);

    _file = allocateTempfile(_filename, totalSize);

    try {
      _table = allocateMemory(numberElements);
      TRI_ASSERT(_table != nullptr);

#ifdef __linux__
      if (numberElements > 1000000) {
        uintptr_t mem = reinterpret_cast<uintptr_t>(_table);
        uintptr_t pageSize = getpagesize();
        mem = (mem / pageSize) * pageSize;
        void* memptr = reinterpret_cast<void*>(mem);
        TRI_MMFileAdvise(memptr, totalSize, TRI_MADVISE_RANDOM);
      }
#endif

      _nrAlloc = static_cast<IndexType>(numberElements);
    } catch (...) {
      deallocateTempfile();
      TRI_ASSERT(_file == -1);
      throw;
    }

    _nrCollisions = 0;
  }

  void deallocate() {
    deallocateMemory();
    deallocateTempfile();

    TRI_ASSERT(_file == -1);
    TRI_ASSERT(_filename == nullptr);
    TRI_ASSERT(_table == nullptr);
  }

 private:
  EntryType* allocateMemory(size_t numberElements) {
    TRI_ASSERT(numberElements > 0);

    if (_file == -1) {
      return new EntryType[numberElements]();
    }

    size_t const totalSize = requiredSize(numberElements);

    // initialize the file
    TRI_ASSERT(_file > 0);
    TRI_ASSERT(_mmHandle == nullptr);

    void* data;
    int flags = MAP_SHARED;
    int res = TRI_MMFile(nullptr, totalSize, PROT_WRITE | PROT_READ, flags,
                         _file, &_mmHandle, 0, &data);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    TRI_ASSERT(data != nullptr);

    try {
      // call placement new constructor
      (void)new (data) EntryType[numberElements]();

      return static_cast<EntryType*>(data);
    } catch (...) {
      TRI_UNMMFile(data, totalSize, _file, &_mmHandle);
      _mmHandle = nullptr;
      throw;
    }
  }

  void deallocateMemory() {
    if (_table == nullptr) {
      TRI_ASSERT(_nrAlloc == 0);
      TRI_ASSERT(_nrUsed == 0);
      return;
    }

    if (_file == -1) {
      delete[] _table;
    } else {
      if (TRI_UNMMFile(_table, requiredSize(_nrAlloc), _file, &_mmHandle) != TRI_ERROR_NO_ERROR) {
        // unmapping failed
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "munmap failed";
      }
      _mmHandle = nullptr;
    }
    _table = nullptr;
    _nrAlloc = 0;
    _nrUsed = 0;
    _nrCollisions = 0;
  }

  int allocateTempfile(char*& filename, size_t filesize) {
    TRI_ASSERT(filename == nullptr);

    return -1;
  }

  void deallocateTempfile() {}
};

}  // namespace basics
}  // namespace arangodb

#endif
