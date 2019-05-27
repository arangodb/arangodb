////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_BUFFER_H
#define ARANGODB_PREGEL_BUFFER_H 1

#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/files.h"
#include "Basics/memory-map.h"
#include "Logger/Logger.h"

#include <cstddef>

#ifdef __linux__
#include <sys/mman.h>
#endif

namespace arangodb {
namespace pregel {

/// continuous memory buffer with a fixed capacity
template <typename T>
struct TypedBuffer {
  static_assert(std::is_default_constructible<T>::value, "");
  
  /// close file (see close() )
  virtual ~TypedBuffer() {}
  TypedBuffer() : _begin(nullptr), _end(nullptr), _capacity(nullptr) {}

  /// end usage of the structure
  virtual void close() = 0;

  /// raw access
  T* begin() const { return _begin; }
  T* end() const { return _end; }

  T& back() const {
    return *(_end - 1);
  }

  /// get size
  size_t size() const {
    return static_cast<size_t>(_end - _begin);
  }
  /// get number of actually mapped bytes
  size_t capacity() const {
    return static_cast<size_t>(_capacity - _begin);
  }
  size_t remainingCapacity() const {
    return static_cast<size_t>(_capacity - _end);
  }
  
  T* appendElement() {
    TRI_ASSERT(_begin <= _end);
    TRI_ASSERT(_end < _capacity);
    return new (_end++) T();
  }
  
  template <typename U = T>
  typename std::enable_if<std::is_trivially_constructible<U>::value>::type
  advance(std::size_t value) {
    TRI_ASSERT((_end + value) <= _capacity);
    _end += value;
  }
  
 private:
  /// don't copy object
  TypedBuffer(const TypedBuffer&) = delete;
  /// don't copy object
  TypedBuffer& operator=(const TypedBuffer&) = delete;

 protected:
  T* _begin; // begin
  T* _end; // current end
  T* _capacity; // max capacity
};

template <typename T>
class VectorTypedBuffer : public TypedBuffer<T> {
 public:
  VectorTypedBuffer(size_t capacity) : TypedBuffer<T>() {
    this->_begin = static_cast<T*>(malloc(sizeof(T) * capacity));
    this->_end = this->_begin;
    this->_capacity = this->_begin + capacity;

    if (this->_begin == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }
  
  ~VectorTypedBuffer() { 
    close();
  }

  void close() override {
    if (this->_begin == nullptr) {
      return;
    }

    // destroy all elements in the buffer
    for (auto* p = this->_begin; p != this->_end; ++p) {
      reinterpret_cast<T*>(p)->~T();
    }

    free(static_cast<void*>(this->_begin));
    this->_begin = nullptr;
    this->_end = nullptr;
    this->_capacity = nullptr;
  }
};

/// Portable read-only memory mapping (Windows and Linux)
/** Filesize limited by size_t, usually 2^32 or 2^64 */
template <typename T>
class MappedFileBuffer : public TypedBuffer<T> {
  std::string _filename;  // underlying filename
  int _fd = -1;           // underlying file descriptor
  void* _mmHandle;        // underlying memory map object handle (windows only)
  size_t _mappedSize;     // actually mapped size
  
 public:

  explicit MappedFileBuffer(size_t capacity) : TypedBuffer<T>() {
    double tt = TRI_microtime();
    long tt2 = random();
    
    std::string file = "pregel_" + std::to_string((uint64_t)tt) + "_" + std::to_string(tt2) + ".mmap";
    this->_filename = basics::FileUtils::buildFilename(TRI_GetTempPath(), file);

    _mappedSize = sizeof(T) * capacity;
    size_t pageSize = PageSizeFeature::getPageSize();
    TRI_ASSERT(pageSize >= 256);
    // use multiples of page-size
    _mappedSize = (size_t)(((_mappedSize + pageSize - 1) / pageSize) * pageSize);
    
    _fd = TRI_CreateDatafile(_filename, _mappedSize);
    if (_fd < 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "pregel cannot create mmap file");
    }

    // memory map the data
    void* data;
    int flags = MAP_SHARED;
#ifdef __linux__
    // try populating the mapping already
    flags |= MAP_POPULATE;
#endif
    int res = TRI_MMFile(0, _mappedSize, PROT_WRITE | PROT_READ, flags, _fd,
                         &_mmHandle, 0, &data);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_set_errno(res);
      TRI_TRACKED_CLOSE_FILE(_fd);
      _fd = -1;

      // remove file
      TRI_UnlinkFile(_filename.c_str());

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("cannot memory map file '") + _filename + "': '" + TRI_errno_string(res) + "'");
    }

    this->_begin = static_cast<T*>(data);
    this->_end = static_cast<T*>(data);
    this->_capacity = this->_begin + capacity;
  }

  /// close file (see close() )
  ~MappedFileBuffer() { close(); }

  void sequentialAccess() {
    TRI_MMFileAdvise(this->_begin, _mappedSize, TRI_MADVISE_SEQUENTIAL);
  }

  void randomAccess() {
    TRI_MMFileAdvise(this->_begin, _mappedSize, TRI_MADVISE_RANDOM);
  }

  void willNeed() {
    TRI_MMFileAdvise(this->_begin, _mappedSize, TRI_MADVISE_WILLNEED);
  }

  void dontNeed() {
    TRI_MMFileAdvise(this->_begin, _mappedSize, TRI_MADVISE_DONTNEED);
  }

  /// close file
  void close() override {
    if (this->_begin == nullptr) {
      // already closed or not opened
      return;
    }

    // destroy all elements in the buffer
    for (auto* p = this->_begin; p != this->_end; ++p) {
      reinterpret_cast<T*>(p)->~T();
    }

    int res = TRI_UNMMFile(this->_begin, _mappedSize, _fd, &_mmHandle);
    if (res != TRI_ERROR_NO_ERROR) {
      // leave file open here as it will still be memory-mapped
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "munmap failed with: " << res;
    }
    if (_fd != -1) {
      TRI_ASSERT(_fd >= 0);
      int res = TRI_TRACKED_CLOSE_FILE(_fd);
      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "unable to close pregel mapped file '" << _filename << "': " << res;
      }
      
      // remove file
      TRI_UnlinkFile(this->_filename.c_str());
    }

    this->_begin = nullptr;
    this->_end = nullptr;
    this->_capacity = nullptr;
    this->_fd = -1;
  }

  /// true, if file successfully opened
  bool isValid() const { return this->_begin != nullptr; }
};
}  // namespace pregel
}  // namespace arangodb

#endif
