////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_ITERATOR_H
#define VELOCYPACK_ITERATOR_H 1

#include <iosfwd>
#include <functional>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Slice.h"
#include "velocypack/ValueType.h"

namespace arangodb {
namespace velocypack {

class ArrayIterator {
 public:
  ArrayIterator() = delete;

  explicit ArrayIterator(Slice const& slice)
      : _slice(slice), _size(_slice.length()), _position(0), _current(nullptr) { 
    if (slice.type() != ValueType::Array) {
      throw Exception(Exception::InvalidValueType, "Expecting Array slice");
    }
    reset();
  }

  ArrayIterator(ArrayIterator const& other) noexcept
      : _slice(other._slice),
        _size(other._size),
        _position(other._position),
        _current(other._current) {}

  ArrayIterator& operator=(ArrayIterator const& other) = delete;
  ArrayIterator& operator=(ArrayIterator&& other) = default;

  // prefix ++
  ArrayIterator& operator++() {
    ++_position;
    if (_position < _size && _current != nullptr) {
      _current += Slice(_current).byteSize();
    } else {
      _current = nullptr;
    }
    return *this;
  }

  // postfix ++
  ArrayIterator operator++(int) {
    ArrayIterator result(*this);
    ++(*this);
    return result;
  }

  bool operator!=(ArrayIterator const& other) const {
    return _position != other._position;
  }

  Slice operator*() const {
    if (_current != nullptr) {
      return Slice(_current);
    }
    return _slice.at(_position);
  }

  ArrayIterator begin() { 
    auto it = ArrayIterator(*this);
    it._position = 0;
    return it;
  }

  ArrayIterator begin() const { 
    auto it = ArrayIterator(*this);
    it._position = 0;
    return it;
  }

  ArrayIterator end() {
    auto it = ArrayIterator(*this);
    it._position = it._size;
    return it;
  }

  ArrayIterator end() const {
    auto it = ArrayIterator(*this);
    it._position = it._size;
    return it;
  }

  inline bool valid() const noexcept { return (_position < _size); }

  inline Slice value() const {
    if (_position >= _size) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    return operator*();
  }

  inline void next() {
    operator++();
  }

  inline ValueLength index() const noexcept { return _position; }

  inline ValueLength size() const noexcept { return _size; }

  inline bool isFirst() const noexcept { return (_position == 0); }

  inline bool isLast() const noexcept { return (_position + 1 >= _size); }

  inline void forward(ValueLength count) {
    if (_position + count >= _size) {
      // beyond end of data
      _current = nullptr;
      _position = _size;
    } else {
      auto h = _slice.head();
      if (h == 0x13) {
        while (count-- > 0) {
          _current += Slice(_current).byteSize();
          ++_position;
        }
      } else {
        _position += count;
        _current = _slice.at(_position).start();
      } 
    }
  }
    
  inline void reset() {
    _position = 0;
    _current = nullptr;
    if (_size > 0) {
      auto h = _slice.head();
      if (h == 0x13) {
        _current = _slice.at(0).start();
      } else {
        _current = _slice.begin() + _slice.findDataOffset(h);
      }
    }
  }

 private:
  Slice _slice;
  ValueLength _size;
  ValueLength _position;
  uint8_t const* _current;
};

class ObjectIterator {
 public:
  struct ObjectPair {
    ObjectPair(Slice const& key, Slice const& value) : key(key), value(value) {}
    Slice const key;
    Slice const value;
  };

  ObjectIterator() = delete;

  // The useSequentialIteration flag indicates whether or not the iteration
  // simply jumps from key/value pair to key/value pair without using the
  // index. The default `false` is to use the index if it is there.
  explicit ObjectIterator(Slice const& slice, bool useSequentialIteration = false)
      : _slice(slice), _size(_slice.length()), _position(0), _current(nullptr),
        _useSequentialIteration(useSequentialIteration) {
    if (!slice.isObject()) {
      throw Exception(Exception::InvalidValueType, "Expecting Object slice");
    }

    if (_size > 0) {
      auto h = slice.head();
      if (h == 0x14) {
        _current = slice.keyAt(0, false).start();
      } else if (useSequentialIteration) {
        _current = slice.begin() + slice.findDataOffset(h);
      }
    }
  }

  ObjectIterator(ObjectIterator const& other) noexcept
      : _slice(other._slice),
        _size(other._size),
        _position(other._position),
        _current(other._current),
        _useSequentialIteration(other._useSequentialIteration) {}

  ObjectIterator& operator=(ObjectIterator const& other) = delete;
  ObjectIterator& operator=(ObjectIterator&& other) = default;

  // prefix ++
  ObjectIterator& operator++() {
    ++_position;
    if (_position < _size && _current != nullptr) {
      // skip over key
      _current += Slice(_current).byteSize();
      // skip over value
      _current += Slice(_current).byteSize();
    } else {
      _current = nullptr;
    }
    return *this;
  }

  // postfix ++
  ObjectIterator operator++(int) {
    ObjectIterator result(*this);
    ++(*this);
    return result;
  }

  bool operator!=(ObjectIterator const& other) const {
    return _position != other._position;
  }

  ObjectPair operator*() const {
    if (_current != nullptr) {
      Slice key = Slice(_current);
      return ObjectPair(key.makeKey(), Slice(_current + key.byteSize()));
    }
    return ObjectPair(_slice.getNthKey(_position, true), _slice.getNthValue(_position));
  }

  ObjectIterator begin() { 
    auto it = ObjectIterator(*this);
    it._position = 0;
    return it;
  }

  ObjectIterator begin() const { 
    auto it = ObjectIterator(*this);
    it._position = 0;
    return it;
  }

  ObjectIterator end() {
    auto it = ObjectIterator(*this);
    it._position = it._size;
    return it;
  }

  ObjectIterator end() const {
    auto it = ObjectIterator(*this);
    it._position = it._size;
    return it;
  }

  inline bool valid() const noexcept { return (_position < _size); }

  inline Slice key(bool translate = true) const {
    if (_position >= _size) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    if (_current != nullptr) {
      Slice s(_current);
      return translate ? s.makeKey() : s;
    }
    return _slice.getNthKey(_position, translate);
  }

  inline Slice value() const {
    if (_position >= _size) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    if (_current != nullptr) {
      Slice key = Slice(_current);
      return Slice(_current + key.byteSize());
    }
    return _slice.getNthValue(_position);
  }

  inline void next() {
    operator++();
  }

  inline ValueLength index() const noexcept { return _position; }

  inline ValueLength size() const noexcept { return _size; }

  inline bool isFirst() const noexcept { return (_position == 0); }

  inline bool isLast() const noexcept { return (_position + 1 >= _size); }

 private:
  Slice _slice;
  ValueLength _size;
  ValueLength _position;
  uint8_t const* _current;
  bool _useSequentialIteration;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

std::ostream& operator<<(std::ostream&,
                         arangodb::velocypack::ArrayIterator const*);

std::ostream& operator<<(std::ostream&,
                         arangodb::velocypack::ArrayIterator const&);

std::ostream& operator<<(std::ostream&,
                         arangodb::velocypack::ObjectIterator const*);

std::ostream& operator<<(std::ostream&,
                         arangodb::velocypack::ObjectIterator const&);

#endif
