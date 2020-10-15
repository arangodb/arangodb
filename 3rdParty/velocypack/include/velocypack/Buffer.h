////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef VELOCYPACK_BUFFER_H
#define VELOCYPACK_BUFFER_H 1

#include <cstring>
#include <cstdlib>
#include <string>
#include <new>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"

namespace arangodb {
namespace velocypack {

template <typename T>
class Buffer {
  static_assert(sizeof(T) == 1, "expecting sizeof(T) to be 1");

 public:
  Buffer() noexcept : _buffer(_local), _capacity(sizeof(_local)), _size(0) {
    poison(_buffer, _capacity);
    initWithNone();
  }

  explicit Buffer(ValueLength expectedLength) : Buffer() {
    reserve(expectedLength);
    initWithNone();
  }

  Buffer(Buffer const& that) : Buffer() {
    if (that._size > 0) {
      if (that._size > sizeof(that._local)) {
        _buffer = static_cast<T*>(velocypack_malloc(checkOverflow(that._size)));
        ensureValidPointer(_buffer);
        _capacity = that._size;
      } else {
        VELOCYPACK_ASSERT(_buffer == &_local[0]);
        _capacity = sizeof(_local);
      }
      memcpy(_buffer, that._buffer, checkOverflow(that._size));
      _size = that._size;
    }
  }

  Buffer& operator=(Buffer const& that) {
    if (this != &that) {
      if (that._size <= _capacity) { 
        // our own buffer is big enough to hold the data
        initWithNone();
        memcpy(_buffer, that._buffer, checkOverflow(that._size));
      } else {
        // our own buffer is not big enough to hold the data
        T* buffer = static_cast<T*>(velocypack_malloc(checkOverflow(that._size)));
        ensureValidPointer(buffer);
        buffer[0] = '\x00';
        memcpy(buffer, that._buffer, checkOverflow(that._size));

        if (_buffer != _local) {
          velocypack_free(_buffer);
        }
        _buffer = buffer;
        _capacity = that._size;
      }

      _size = that._size;
    }
    return *this;
  }

  Buffer(Buffer&& that) noexcept : _buffer(_local), _capacity(sizeof(_local)) {
    poison(_buffer, _capacity);
    initWithNone();
    if (that._buffer == that._local) {
      VELOCYPACK_ASSERT(that._capacity == sizeof(that._local));
      memcpy(_buffer, that._buffer, checkOverflow(that._size));
    } else {
      _buffer = that._buffer;
      _capacity = that._capacity;
      that._buffer = that._local;
      that._capacity = sizeof(that._local);
    }
    _size = that._size;
    that._size = 0;
    that.initWithNone();
  }

  Buffer& operator=(Buffer&& that) noexcept {
    if (this != &that) {
      if (_buffer != _local) {
        velocypack_free(_buffer);
      }
      if (that._buffer == that._local) {
        _buffer = _local;
        _capacity = sizeof(_local);
        initWithNone();
        memcpy(_buffer, that._buffer, checkOverflow(that._size));
      } else {
        _buffer = that._buffer;
        _capacity = that._capacity;
        that._buffer = that._local;
        that._capacity = sizeof(that._local);
      }
      _size = that._size;
      that._size = 0;
      that.initWithNone();
    }
    return *this;
  }

  ~Buffer() { 
    if (_buffer != _local) {
      velocypack_free(_buffer);
    }
  }

  inline T* data() noexcept { return _buffer; }
  inline T const* data() const noexcept { return _buffer; }

  inline bool empty() const noexcept { return _size == 0; }
  inline ValueLength size() const noexcept { return _size; }
  inline ValueLength length() const noexcept { return _size; }
  inline ValueLength byteSize() const noexcept { return _size; }
  
  inline ValueLength capacity() const noexcept { return _capacity; }

  std::string toString() const {
    return std::string(reinterpret_cast<char const*>(_buffer), _size);
  }

  void reset() noexcept { 
    _size = 0;
    initWithNone();
  }

  void resetTo(ValueLength position) {
    if (position > _capacity) { 
      throw Exception(Exception::IndexOutOfBounds);
    }
    _size = position;
  }

  // move internal buffer position one byte ahead
  inline void advance() noexcept {
    advance(1);
  }
  
  // move internal buffer position n bytes ahead
  inline void advance(std::size_t value) noexcept {
    VELOCYPACK_ASSERT(_size <= _capacity);
    VELOCYPACK_ASSERT(_size + value <= _capacity);
    _size += value;
  }
  
  // move internal buffer position n bytes backward
  inline void rollback(std::size_t value) noexcept {
    VELOCYPACK_ASSERT(_size <= _capacity);
    VELOCYPACK_ASSERT(_size >= value);
    _size -= value;
  }

  void clear() noexcept {
    _size = 0;
    if (_buffer != _local) {
      velocypack_free(_buffer);
      _buffer = _local;
      _capacity = sizeof(_local);
      poison(_buffer, _capacity);
    }
    initWithNone();
  }

  // Steal external memory; only allowed when the buffer is not local,
  // i.e. !usesLocalMemory()
   T* steal() noexcept {
    VELOCYPACK_ASSERT(!usesLocalMemory());

    auto buffer = _buffer;
    _buffer = _local;
    _size = 0;
    _capacity = sizeof(_local);
    poison(_buffer, _capacity);
    initWithNone();

    return buffer;
  }

  inline T& operator[](std::size_t position) noexcept {
    return _buffer[position];
  }

  inline T const& operator[](std::size_t position) const noexcept {
    return _buffer[position];
  }
  
  inline T& at(std::size_t position) {
    if (position >= _size) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    return operator[](position);
  }
  
  inline T const& at(std::size_t position) const {
    if (position >= _size) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    return operator[](position);
  }

  inline void push_back(char c) {
    reserve(1);
    _buffer[_size++] = c;
  }
  
  void append(uint8_t const* p, ValueLength len) {
    reserve(len);
    memcpy(_buffer + _size, p, checkOverflow(len));
    _size += len;
  }

  void append(char const* p, ValueLength len) {
    reserve(len);
    memcpy(_buffer + _size, p, checkOverflow(len));
    _size += len;
  }
  
  void append(std::string const& value) {
    return append(value.data(), value.size());
  }
  
  void append(Buffer<T> const& value) {
    return append(value.data(), value.size());
  }

  // reserves len *extra* bytes of storage space
  // this should probably be renamed to reserveExtra
  inline void reserve(ValueLength len) {
    VELOCYPACK_ASSERT(_size <= _capacity);

    if (_size + len >= _capacity) {
      grow(len);
    }
  }

  // If true, uses memory inside the buffer (_local).
  // Otherwise, uses memory on the heap.
  bool usesLocalMemory() const noexcept {
    return _buffer == _local;
  }
 
 private:
  // initialize Buffer with a None value
  inline void initWithNone() noexcept { _buffer[0] = '\x00'; }

  inline void ensureValidPointer(T* ptr) const {
    if (VELOCYPACK_UNLIKELY(ptr == nullptr)) {
      throw std::bad_alloc();
    }
  }
  
  // poison buffer memory, used only for debugging
#ifdef VELOCYPACK_DEBUG
  inline void poison(T* p, ValueLength length) noexcept {
    memset(p, 0xa5, length);
  }
#else
  inline void poison(T*, ValueLength) noexcept {}
#endif

  void grow(ValueLength len) {
    VELOCYPACK_ASSERT(_size + len >= sizeof(_local));

    // need reallocation
    ValueLength newLen = _size + len;
    constexpr double growthFactor = 1.25;
    if (newLen < growthFactor * _size) {
      // ensure the buffer grows sensibly and not by 1 byte only
      newLen = static_cast<ValueLength>(growthFactor * _size);
    }
    VELOCYPACK_ASSERT(newLen > _size);

    // intentionally do not initialize memory here
    // intentionally also do not care about alignments here, as we
    // expect T to be 1-byte-aignable
    VELOCYPACK_ASSERT(newLen > 0);
    T* p;
    if (_buffer != _local) {
      p = static_cast<T*>(velocypack_realloc(_buffer, checkOverflow(newLen)));
      ensureValidPointer(p);
      // realloc will have copied the old data
    } else {
      p = static_cast<T*>(velocypack_malloc(checkOverflow(newLen)));
      ensureValidPointer(p);
      // copy existing data into buffer
      memcpy(p, _buffer, checkOverflow(_size));
    }
    poison(p + _capacity, newLen - _capacity);

    _buffer = p;
    _capacity = newLen;
    
    VELOCYPACK_ASSERT(_size <= _capacity);
  }
  
  T* _buffer;
  ValueLength _capacity;
  ValueLength _size;

  // an already allocated space for small values
  T _local[192];
};

typedef Buffer<char> CharBuffer;
typedef Buffer<uint8_t> UInt8Buffer;

template<typename T>
struct BufferNonDeleter {
  void operator()(Buffer<T>*) {}
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
