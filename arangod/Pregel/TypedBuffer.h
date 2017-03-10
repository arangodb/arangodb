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

#ifndef ARANGODB_PREGEL_MMAP_H
#define ARANGODB_PREGEL_MMAP_H 1

#include <cstddef>
#include "Basics/Common.h"

namespace arangodb {
namespace pregel {
  
template<typename T>
class TypedBuffer {
  /// close file (see close() )
  virtual ~TypedBuffer() {};
  
  /// @brief return whether the datafile is a physical file (true) or an
  /// anonymous mapped region (false)
  //inline bool isPhysical() const { return !_filename.empty(); }
  
  /// close datafile
  void close();
  
  /// raw access
  virtual T* getData() const = 0;
  
  /// true, if file successfully opened
  bool isValid() const = 0;
  
  /// get file size
  //uint64_t size() const;
  /// get number of actually mapped bytes
  size_t size() const = 0;
  
  /// replace mapping by a new one of the same file, offset MUST be a multiple
  /// of the page size
  bool resize(size_t newSize) = 0;
};
  
template<typename T>
class VectorTypedBuffer : public TypedBuffer<T> {
  std::vector<T> _data;
public:
  VectorTypedBuffer(size_t entries) : _data(entries) {}
  
  void close() override {
    //_data.clear();
  }
  
  /// raw access
  virtual T* getData() const override {
    return _data.data();
  };
  
  /// true, if file successfully opened
  bool isValid() const {
    return true;
  };
  
  /// get file size
  //uint64_t size() const;
  /// get number of actually mapped bytes
  size_t size() const override {
    return _data.size();
  }
  
  /// replace mapping by a new one of the same file, offset MUST be a multiple
  /// of the page size
  bool resize(size_t newSize)
};

/// Portable read-only memory mapping (Windows and Linux)
/** Filesize limited by size_t, usually 2^32 or 2^64 */
template<typename T>
class MappedFileBuffer {
 public:
  
  /// close file (see close() )
  ~MappedFileBuffer();

  /// @brief return whether the datafile is a physical file (true) or an
  /// anonymous mapped region (false)
  inline bool isPhysical() const { return !_filename.empty(); }

  /// close file
  void close();

  /// raw access
  T* getData() const override {
    return _data;
  }

  /// true, if file successfully opened
  bool isValid() const override {
    return _data != nullptr;
  }

  /// get file size
  //uint64_t size() const;
  /// get number of actually mapped bytes
  size_t size() const override {
    return _size;
  }

  /// replace mapping by a new one of the same file, offset MUST be a multiple
  /// of the page size
  bool resize(size_t newSize);

 private:
  
  TypedBuffer(size_t entries, T* data) : _state(TypedBufferState::IN_MEMORY),
  _size(entries), _data(data) {}
  
  TypedBuffer(std::string const& filename, int fd, void* mmHandle, size_t entries, T* data)
  : _state(TypedBufferState::MEMORY_MAPPED_FILE),
  _filename(filename), _fd(fd),
#ifdef _MSC_VER
  _mmHandle(mmHandle),
#endif
  _size(entries), _data(data) {}
  
  /// don't copy object
  TypedBuffer(const TypedBuffer&);
  /// don't copy object
  TypedBuffer& operator=(const TypedBuffer&);

  /// get OS page size (for remap)
  //static int getpagesize();

  TypedBufferState _state = TypedBufferState::CLOSED;
  std::string _filename;     // underlying filename
  int _fd = -1;              // underlying file descriptor
#ifdef _MSC_VER
  void* _mmHandle;  // underlying memory map object handle (windows only)
#endif
  size_t _size = 0;
  T* _data = nullptr;  // start of the data array
};
}
}

#endif
