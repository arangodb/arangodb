////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstddef>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef __linux__
#include <sys/mman.h>
#endif

#include "Basics/Common.h"

#include "Basics/FileUtils.h"
#include "Basics/PageSize.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/memory-map.h"
#include "Basics/operating-system.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"

namespace arangodb {
namespace pregel {

/// continuous memory buffer with a fixed capacity
template <typename T>
struct TypedBuffer {
  static_assert(std::is_default_constructible<T>::value,
                "Stored type T has to be default constructible");

  /// close file (see close() )
  virtual ~TypedBuffer() = default;
  TypedBuffer() noexcept : _begin(nullptr), _end(nullptr), _capacity(nullptr) {}

  /// end usage of the structure
  virtual void close() = 0;

  /// raw access
  T* begin() const noexcept { return _begin; }
  T* end() const noexcept { return _end; }

  T& back() const noexcept { return *(_end - 1); }

  /// get size
  size_t size() const noexcept {
    TRI_ASSERT(_end >= _begin);
    return static_cast<size_t>(_end - _begin);
  }
  /// get number of actually mapped bytes
  size_t capacity() const noexcept {
    TRI_ASSERT(_capacity >= _begin);
    return static_cast<size_t>(_capacity - _begin);
  }
  size_t remainingCapacity() const noexcept {
    TRI_ASSERT(_capacity >= _end);
    return static_cast<size_t>(_capacity - _end);
  }

  /// move end cursor, calls placement new
  T* appendElement() {
    TRI_ASSERT(_begin <= _end);
    TRI_ASSERT(_end < _capacity);
    return new (_end++) T();
  }

  template <typename U = T>
  typename std::enable_if<std::is_trivially_constructible<U>::value>::type advance(std::size_t value) noexcept {
    TRI_ASSERT((_end + value) <= _capacity);
    _end += value;
  }

 private:
  /// don't copy object
  TypedBuffer(TypedBuffer const&) = delete;
  /// don't copy object
  TypedBuffer& operator=(TypedBuffer const&) = delete;

 protected:
  T* _begin;     // begin
  T* _end;       // current end
  T* _capacity;  // max capacity
};

template <typename T>
class VectorTypedBuffer : public TypedBuffer<T> {
 public:
  explicit VectorTypedBuffer(size_t capacity) : TypedBuffer<T>() {
    TRI_ASSERT(capacity > 0);
    this->_begin = static_cast<T*>(malloc(sizeof(T) * capacity));
    this->_end = this->_begin;
    this->_capacity = this->_begin + capacity;

    if (this->_begin == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  ~VectorTypedBuffer() { close(); }

  // cppcheck-suppress virtualCallInConstructor
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
  std::string _logPrefix;  // prefix used for logging
  std::string _filename;   // underlying filename
  int _fd;                 // underlying file descriptor
  bool _temporary;         // O_TMPFILE used?
  void* _mmHandle;         // underlying memory map object handle (windows only)
  size_t _mappedSize;      // actually mapped size

 public:
  MappedFileBuffer(MappedFileBuffer const& other) = delete;
  MappedFileBuffer& operator=(MappedFileBuffer const& other) = delete;

  explicit MappedFileBuffer(size_t capacity, std::string const& logPrefix)
      : TypedBuffer<T>(),
        _logPrefix(logPrefix),
        _fd(-1),
        _temporary(false),
        _mmHandle(nullptr),
        _mappedSize(sizeof(T) * capacity) {
    TRI_ASSERT(capacity > 0u);

    size_t pageSize = PageSize::getValue();
    TRI_ASSERT(pageSize >= 256);
    // use multiples of page-size
    _mappedSize = (size_t)(((_mappedSize + pageSize - 1) / pageSize) * pageSize);

    _fd = createFile(_mappedSize);

    if (_fd < 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_SYS_ERROR,
          basics::StringUtils::concatT("pregel cannot create mmap file ",
                                       label(), ": ", TRI_last_error()));
    }

    // memory map the data
    void* data;
    int flags = MAP_SHARED;
#ifdef __linux__
    // try populating the mapping already
    flags |= MAP_POPULATE;
#endif
    auto res = TRI_MMFile(0, _mappedSize, PROT_WRITE | PROT_READ, flags, _fd,
                          &_mmHandle, 0, &data);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_set_errno(res);
      TRI_CLOSE(_fd);
      _fd = -1;

      LOG_TOPIC("54dfb", ERR, arangodb::Logger::PREGEL)
          << _logPrefix << "cannot memory map " << label() << ": '"
          << TRI_errno_string(res) << "'";
      LOG_TOPIC("1a034", ERR, arangodb::Logger::PREGEL)
          << _logPrefix
          << "The database directory might reside on a shared folder "
             "(VirtualBox, VMWare) or an NFS-mounted volume which does not "
             "allow memory mapped files.";

      removeFile();

      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          basics::StringUtils::concatT("cannot memory map file ", label(), ": ",
                                       TRI_errno_string(res)));
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

  /// close file
  // cppcheck-suppress virtualCallInConstructor
  void close() override {
    if (this->_begin == nullptr) {
      // already closed or not opened
      return;
    }

    LOG_TOPIC("45530", DEBUG, Logger::PREGEL) << _logPrefix << "closing mmap " << label();

    // destroy all elements in the buffer
    for (auto* p = this->_begin; p != this->_end; ++p) {
      reinterpret_cast<T*>(p)->~T();
    }

    if (auto res = TRI_UNMMFile(this->_begin, _mappedSize, _fd, &_mmHandle);
        res != TRI_ERROR_NO_ERROR) {
      // leave file open here as it will still be memory-mapped
      LOG_TOPIC("ab7be", ERR, arangodb::Logger::PREGEL)
          << _logPrefix << "munmap failed with: " << res;
    }
    if (_fd != -1) {
      TRI_ASSERT(_fd >= 0);
      if (auto res = TRI_CLOSE(_fd); res != 0) {
        LOG_TOPIC("00e1d", WARN, arangodb::Logger::PREGEL)
            << _logPrefix << "unable to close pregel mapped " << label() << ": " << res;
      }

      removeFile();
      _filename.clear();
    }

    this->_begin = nullptr;
    this->_end = nullptr;
    this->_capacity = nullptr;
    this->_fd = -1;
  }

  /// true, if file successfully opened
  bool isValid() const { return this->_begin != nullptr; }

 private:
  std::string label() const {
    if (_temporary) {
      return "temporary file in " + _filename;
    }
    return "file " + _filename;
  }

  std::string buildFilename(bool temporary) const {
    if (temporary) {
      // only need a path
      return TRI_GetTempPath();
    }

    double tt = TRI_microtime();
    int64_t tt2 = arangodb::RandomGenerator::interval((int64_t)0LL, (int64_t)0x7fffffffffffffffLL);

    std::string file =
        "pregel-" + std::to_string(uint64_t(Thread::currentProcessId())) + "-" +
        std::to_string(uint64_t(tt)) + "-" + std::to_string(tt2) + ".mmap";
    return basics::FileUtils::buildFilename(TRI_GetTempPath(), file);
  }

  void removeFile() const {
    if (!_temporary && !_filename.empty()) {
      TRI_UnlinkFile(_filename.c_str());
    }
  }

  /// @brief creates a new datafile
  /// returns the file descriptor or -1 if the file cannot be created
  int createFile(size_t maximalSize) {
    TRI_ERRORBUF;

#ifdef _WIN32
    bool temporary = false;
#else
    bool temporary = true;
#endif

    // open the file
    int fd = -1;
    if (temporary) {
      _temporary = true;
      _filename = buildFilename(_temporary);
      // try creating a temporary file with O_TMPFILE first.
      // this may be unsupported.
      // in that case, we will fall back to creating a regular (non-temp) file.
      fd = TRI_CREATE(_filename.c_str(),
                      O_EXCL | O_RDWR | TRI_O_CLOEXEC | TRI_NOATIME | TRI_O_TMPFILE,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
      // if fd is < 0, we will try without O_TMPFILE below.
    }

    if (fd < 0) {
      _temporary = false;
      _filename = buildFilename(_temporary);
      fd = TRI_CREATE(_filename.c_str(), O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC | TRI_NOATIME,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    }

    LOG_TOPIC("358e3", DEBUG, Logger::PREGEL)
        << _logPrefix << "creating mmap " << label() << " of " << _mappedSize
        << " bytes capacity";

    TRI_IF_FAILURE("CreateDatafile1") {
      // intentionally fail
      TRI_CLOSE(fd);
      fd = -1;
      errno = ENOSPC;
    }

    if (fd < 0) {
      if (errno == ENOSPC) {
        TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
        LOG_TOPIC("f7530", ERR, arangodb::Logger::PREGEL)
            << _logPrefix << "cannot create " << label() << ": " << TRI_last_error();
      } else {
        TRI_SYSTEM_ERROR();

        TRI_set_errno(TRI_ERROR_SYS_ERROR);
        LOG_TOPIC("53a75", ERR, arangodb::Logger::PREGEL)
            << _logPrefix << "cannot create " << label() << ": " << TRI_GET_ERRORBUF;
      }

      _filename.clear();
      return -1;
    }

    // no fallocate present, or at least pretend it's not there...
    int res = 1;

#ifdef __linux__
#ifdef FALLOC_FL_ZERO_RANGE
    // try fallocate
    res = fallocate(fd, FALLOC_FL_ZERO_RANGE, 0, maximalSize);
#endif
#endif

    // cppcheck-suppress knownConditionTrueFalse
    if (res != 0) {
      // either fallocate failed or it is not there...

      // create a buffer filled with zeros
      static constexpr size_t nullBufferSize = 4096;
      char nullBuffer[nullBufferSize];
      memset(&nullBuffer[0], 0, nullBufferSize);

      // fill file with zeros from buffer
      size_t writeSize = nullBufferSize;
      size_t written = 0;
      while (written < maximalSize) {
        if (writeSize + written > maximalSize) {
          writeSize = maximalSize - written;
        }

        ssize_t writeResult =
            TRI_WRITE(fd, &nullBuffer[0], static_cast<TRI_write_t>(writeSize));

        TRI_IF_FAILURE("CreateDatafile2") {
          // intentionally fail
          writeResult = -1;
          errno = ENOSPC;
        }

        if (writeResult < 0) {
          if (errno == ENOSPC) {
            TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
            LOG_TOPIC("449cf", ERR, arangodb::Logger::PREGEL)
                << _logPrefix << "cannot create " << label() << ": "
                << TRI_last_error();
          } else {
            TRI_SYSTEM_ERROR();
            TRI_set_errno(TRI_ERROR_SYS_ERROR);
            LOG_TOPIC("2c4a6", ERR, arangodb::Logger::PREGEL)
                << _logPrefix << "cannot create " << label() << ": " << TRI_GET_ERRORBUF;
          }

          TRI_CLOSE(fd);
          removeFile();

          return -1;
        }

        written += static_cast<size_t>(writeResult);
      }
    }

    // go back to offset 0
    TRI_lseek_t offset = TRI_LSEEK(fd, (TRI_lseek_t)0, SEEK_SET);

    if (offset == (TRI_lseek_t)-1) {
      TRI_SYSTEM_ERROR();
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      TRI_CLOSE(fd);

      LOG_TOPIC("dfc52", ERR, arangodb::Logger::PREGEL)
          << _logPrefix << "cannot seek in " << label() << ": " << TRI_GET_ERRORBUF;

      removeFile();
      _filename.clear();

      return -1;
    }

    return fd;
  }
};
}  // namespace pregel
}  // namespace arangodb
