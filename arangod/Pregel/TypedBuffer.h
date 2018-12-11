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
#include "Basics/files.h"
#include "Basics/memory-map.h"
#include "Logger/Logger.h"

#include <cstddef>

#ifdef __linux__
#include <sys/mman.h>
#endif

namespace arangodb {
namespace pregel {

template <typename T>
struct TypedBuffer {
  /// close file (see close() )
  virtual ~TypedBuffer() {}
  TypedBuffer() : _ptr(nullptr) {}

  /// @brief return whether the datafile is a physical file (true) or an
  /// anonymous mapped region (false)
  // inline bool isPhysical() const { return !_filename.empty(); }

  /// close datafile
  virtual void close() = 0;

  /// raw access
  T* data() const { return _ptr; }

  virtual T& back() = 0;

  /// get file size
  // uint64_t size() const;
  /// get number of actually mapped bytes
  virtual size_t size() const = 0;

  /// replace mapping by a new one of the same file, offset MUST be a multiple
  /// of the page size
  virtual void resize(size_t newSize) = 0;

 private:
  /// don't copy object
  TypedBuffer(const TypedBuffer&) = delete;
  /// don't copy object
  TypedBuffer& operator=(const TypedBuffer&) = delete;

 protected:
  T* _ptr;
};

template <typename T>
class VectorTypedBuffer : public TypedBuffer<T> {
  std::vector<T> _vector;

 public:
  VectorTypedBuffer(size_t entries) : _vector(entries) {
    this->_ptr = _vector.data();
  }

  void close() override {
    //_data.clear();
  }

  T& back() override { return _vector.back(); }

  /// get file size
  // uint64_t size() const;
  /// get number of actually mapped bytes
  size_t size() const override { return _vector.size(); }

  /// replace mapping by a new one of the same file, offset MUST be a multiple
  /// of the page size
  virtual void resize(size_t newSize) override { _vector.resize(newSize); }

  void appendEmptyElement() {
    _vector.push_back(T());
    this->_ptr = _vector.data();  // might change address
  }
};

/// Portable read-only memory mapping (Windows and Linux)
/** Filesize limited by size_t, usually 2^32 or 2^64 */
template <typename T>
class MappedFileBuffer : public TypedBuffer<T> {
 public:
#ifdef TRI_HAVE_ANONYMOUS_MMAP
  explicit MappedFileBuffer(size_t entries) : _size(entries) {
#ifdef TRI_MMAP_ANONYMOUS
    // fd -1 is required for "real" anonymous regions
    _fd = -1;
    int flags = TRI_MMAP_ANONYMOUS | MAP_SHARED;
#else
    // ugly workaround if MAP_ANONYMOUS is not available
    _fd = TRI_OPEN("/dev/zero", O_RDWR | TRI_O_CLOEXEC);
    if (_fd == -1) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    int flags = MAP_PRIVATE;
#endif

    // memory map the data
    _mappedSize = sizeof(T) * entries;
    void* ptr;
    int res = TRI_MMFile(nullptr, _mappedSize, PROT_WRITE | PROT_READ, flags,
                         _fd, &_mmHandle, 0, &ptr);
#ifdef MAP_ANONYMOUS
// nothing to do
#else
    // close auxilliary file
    TRI_CLOSE(_fd);
    _fd = -1;
#endif

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_set_errno(res);

      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "cannot memory map anonymous region: " << TRI_last_error();
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "The database directory might reside on a shared folder "
             "(VirtualBox, VMWare) or an NFS "
             "mounted volume which does not allow memory mapped files.";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    this->_ptr = (T*)ptr;
    // return new TypedBuffer(StaticStrings::Empty, fd, mmHandle, initialSize,
    // ptr);
  }
#else
  explicit MappedFileBuffer(size_t entries) : _size(entries) {
    double tt = TRI_microtime();
    std::string file = "pregel_" + std::to_string((uint64_t)tt) + ".mmap";
    std::string filename = FileUtils::buildFilename(TRI_GetTempPath(), file);

    _mappedSize = sizeof(T) * _size;
    _fd = TRI_CreateDatafile(filename, _mappedSize);
    if (_fd < 0) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
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
      TRI_CLOSE(fd);

      // remove empty file
      TRI_UnlinkFile(filename.c_str());

      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot memory map file '"
                                              << filename << "': '"
                                              << TRI_errno_string(res) << "'";
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "The database directory might reside on a shared folder "
             "(VirtualBox, VMWare) or an NFS-mounted volume which does not "
             "allow memory mapped files.";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    this->_ptr = (T*)data;
  }
#endif

  /// close file (see close() )
  ~MappedFileBuffer() { close(); }

  /// @brief return whether the datafile is a physical file (true) or an
  /// anonymous mapped region (false)
  inline bool isPhysical() const { return !_filename.empty(); }

  void sequentialAccess() {
    TRI_MMFileAdvise(this->_ptr, _mappedSize, TRI_MADVISE_SEQUENTIAL);
  }

  void randomAccess() {
    TRI_MMFileAdvise(this->_ptr, _mappedSize, TRI_MADVISE_RANDOM);
  }

  void willNeed() {
    TRI_MMFileAdvise(this->_ptr, _mappedSize, TRI_MADVISE_WILLNEED);
  }

  void dontNeed() {
    TRI_MMFileAdvise(this->_ptr, _mappedSize, TRI_MADVISE_DONTNEED);
  }

  /// close file
  void close() override {
    if (this->_ptr == nullptr) {
      // already closed or not opened
      return;
    }

    int res = TRI_UNMMFile(this->_ptr, _mappedSize, _fd, &_mmHandle);
    if (res != TRI_ERROR_NO_ERROR) {
      // leave file open here as it will still be memory-mapped
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "munmap failed with: " << res;
    }
    if (isPhysical()) {
      TRI_ASSERT(_fd >= 0);
      int res = TRI_CLOSE(_fd);
      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "unable to close pregel mapped file '" << _filename
            << "': " << res;
      }
    }

    this->_ptr = nullptr;
    _fd = -1;
  }

  T& back() override { return *(this->_ptr + _size - 1); }

  /// true, if file successfully opened
  bool isValid() const { return this->_ptr != nullptr; }

  /// get file size
  // uint64_t size() const;
  /// get number of actually mapped bytes
  size_t size() const override { return _size; }

  /// replace mapping by a new one of the same file, offset MUST be a multiple
  /// of the page size
  void resize(size_t newSize) override {
    if (this->_ptr == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    } else if (newSize == _size) {
      return;
    }

#ifdef __linux__
    size_t newMappedSize = sizeof(T) * newSize;
    void* newPtr =
        mremap((void*)this->_ptr, _mappedSize, newMappedSize, MREMAP_MAYMOVE);
    if (newPtr != MAP_FAILED) {  // success
      TRI_ASSERT(this->_ptr != nullptr);
      this->_ptr = (T*)newPtr;
      _mappedSize = newMappedSize;
      _size = newSize;
      return;
    }
    if (errno == ENOMEM) {
      LOG_TOPIC(DEBUG, Logger::MMAP) << "out of memory in mmap";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY_MMAP);
    } else {
      // preserve errno value while we're logging
      LOG_TOPIC(WARN, Logger::MMAP) << "memory-mapping failed";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SYS_ERROR);
    }
#else
    // resizing mappings doesn't exist on other systems
    if (_size < newSize || newSize * sizeof(T) <= _mappedSize) {
      _size = newSize;
    } else {
      LOG_TOPIC(ERR, Logger::MMAP)
          << "Resizing mmap not supported on this platform";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
    }
#endif
  }

  /// get OS page size (for remap)
  /*int TypedBuffer::getpagesize() {
   #ifdef _MSC_VER
   SYSTEM_INFO sysInfo;
   GetSystemInfo(&sysInfo);
   return sysInfo.dwAllocationGranularity;
   #else
   return sysconf(_SC_PAGESIZE);  //::getpagesize();
   #endif
   }*/

 private:
  /// get OS page size (for remap)
  // static int getpagesize();

  std::string _filename;  // underlying filename
  int _fd = -1;           // underlying file descriptor
  void* _mmHandle;        // underlying memory map object handle (windows only)
  size_t _size = 0;
  size_t _mappedSize;
};
}
}

#endif
