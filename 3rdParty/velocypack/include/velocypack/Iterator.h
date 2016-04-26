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

  ArrayIterator(Slice const& slice)
      : _slice(slice), _size(_slice.length()), _position(0), _current(nullptr) {
    if (slice.type() != ValueType::Array) {
      throw Exception(Exception::InvalidValueType, "Expecting Array slice");
    }

    if (slice.head() == 0x13 && slice.length() > 0) {
      _current = slice.at(0).start();
    }
  }

  ArrayIterator(ArrayIterator const& other)
      : _slice(other._slice),
        _size(other._size),
        _position(other._position),
        _current(other._current) {}

  ArrayIterator& operator=(ArrayIterator const& other) {
    _slice = other._slice;
    _size = other._size;
    _position = other._position;
    _current = other._current;
    return *this;
  }

  // prefix ++
  ArrayIterator& operator++() {
    ++_position;
    if (_position <= _size && _current != nullptr) {
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

  ArrayIterator begin() { return ArrayIterator(_slice); }

  ArrayIterator begin() const { return ArrayIterator(_slice); }

  ArrayIterator end() {
    auto it = ArrayIterator(_slice);
    it._position = it._size;
    return it;
  }

  ArrayIterator end() const {
    auto it = ArrayIterator(_slice);
    it._position = it._size;
    return it;
  }

  inline bool valid() const throw() { return (_position < _size); }

  inline Slice value() const {
    if (_position >= _size) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    return operator*();
  }

  inline bool next() throw() {
    operator++();
    return valid();
  }

  inline ValueLength index() const throw() { return _position; }

  inline ValueLength size() const throw() { return _size; }

  inline bool isFirst() const throw() { return (_position == 0); }

  inline bool isLast() const throw() { return (_position + 1 >= _size); }

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

  ObjectIterator(Slice const& slice)
      : _slice(slice), _size(_slice.length()), _position(0), _current(nullptr) {
    if (slice.type() != ValueType::Object) {
      throw Exception(Exception::InvalidValueType, "Expecting Object slice");
    }

    if (slice.head() == 0x14 && slice.length() > 0) {
      _current = slice.keyAt(0, false).start();
    }
  }

  ObjectIterator(ObjectIterator const& other)
      : _slice(other._slice),
        _size(other._size),
        _position(other._position),
        _current(other._current) {}

  ObjectIterator& operator=(ObjectIterator const& other) {
    _slice = other._slice;
    _size = other._size;
    _position = other._position;
    _current = other._current;
    return *this;
  }

  // prefix ++
  ObjectIterator& operator++() {
    ++_position;
    if (_position <= _size && _current != nullptr) {
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
      return ObjectPair(key, Slice(_current + key.byteSize()));
    }
    return ObjectPair(_slice.keyAt(_position), _slice.valueAt(_position));
  }

  ObjectIterator begin() { return ObjectIterator(_slice); }

  ObjectIterator begin() const { return ObjectIterator(_slice); }

  ObjectIterator end() {
    auto it = ObjectIterator(_slice);
    it._position = it._size;
    return it;
  }

  ObjectIterator end() const {
    auto it = ObjectIterator(_slice);
    it._position = it._size;
    return it;
  }

  inline bool valid() const throw() { return (_position < _size); }

  inline Slice key(bool translate = true) const {
    if (_position >= _size) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    if (_current != nullptr) {
      return Slice(_current);
    }
    return _slice.keyAt(_position, translate);
  }

  inline Slice value() const {
    if (_position >= _size) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    if (_current != nullptr) {
      Slice key = Slice(_current);
      return Slice(_current + key.byteSize());
    }
    return _slice.valueAt(_position);
  }

  inline bool next() throw() {
    operator++();
    return valid();
  }

  inline ValueLength index() const throw() { return _position; }

  inline ValueLength size() const throw() { return _size; }

  inline bool isFirst() const throw() { return (_position == 0); }

  inline bool isLast() const throw() { return (_position + 1 >= _size); }

 private:
  Slice _slice;
  ValueLength _size;
  ValueLength _position;
  uint8_t const* _current;
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
