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

#ifndef VELOCYPACK_ITERATOR_H
#define VELOCYPACK_ITERATOR_H 1

#include <iosfwd>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Slice.h"
#include "velocypack/ValueType.h"

namespace arangodb {
namespace velocypack {

class ArrayIterator : public std::iterator<std::forward_iterator_tag, Slice> {
 public:
  using iterator_category = std::forward_iterator_tag;

  struct Empty {};

  ArrayIterator() = delete;

  // optimization for an empty array
  explicit ArrayIterator(Empty) noexcept
      : _slice(Slice::emptyArraySlice()), _size(0), _position(0), _current(nullptr), _first(nullptr) {}

  explicit ArrayIterator(Slice slice)
      : _slice(slice), _size(0), _position(0), _current(nullptr), _first(nullptr) {
    
    uint8_t const head = slice.head();     

    if (VELOCYPACK_UNLIKELY(slice.type(head) != ValueType::Array)) {
      throw Exception(Exception::InvalidValueType, "Expecting Array slice");
    }

    _size = slice.arrayLength();

    if (_size > 0) {
      VELOCYPACK_ASSERT(head != 0x01); // no empty array allowed here
      if (head == 0x13) {
        _current = slice.start() + slice.getStartOffsetFromCompact();
      } else {
        _current = slice.begin() + slice.findDataOffset(head);
      }
      _first = _current;
    }
  }

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

  bool operator==(ArrayIterator const& other) const noexcept {
    return _position == other._position;
  }
  bool operator!=(ArrayIterator const& other) const noexcept {
    return !operator==(other);
  }

  Slice operator*() const {
    if (_current != nullptr) {
      return Slice(_current);
    }
    // intentionally no out-of-bounds checking here, as it will
    // be performed by Slice::getNthOffset()
    return Slice(_slice.begin() + _slice.getNthOffset(_position));
  }
  
  ArrayIterator begin() const { 
    auto it = ArrayIterator(*this);
    it._position = 0;
    return it;
  }
  
  ArrayIterator end() const {
    auto it = ArrayIterator(*this);
    it._position = it._size;
    return it;
  }

  inline bool valid() const noexcept { return (_position < _size); }

  inline Slice value() const {
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
    _current = _first;
  }

 private:
  Slice _slice;
  ValueLength _size;
  ValueLength _position;
  uint8_t const* _current;
  uint8_t const* _first;
};

struct ObjectIteratorPair {
  ObjectIteratorPair(Slice key, Slice value) noexcept
      : key(key), value(value) {}
  Slice const key;
  Slice const value;
};

class ObjectIterator : public std::iterator<std::forward_iterator_tag, ObjectIteratorPair> {
 public:
  using ObjectPair = ObjectIteratorPair;

  ObjectIterator() = delete;

  // The useSequentialIteration flag indicates whether or not the iteration
  // simply jumps from key/value pair to key/value pair without using the
  // index. The default `false` is to use the index if it is there.
  explicit ObjectIterator(Slice slice, bool useSequentialIteration = false)
      : _slice(slice), _size(0), _position(0), _current(nullptr), _first(nullptr) {
    
    uint8_t const head = slice.head();     

    if (VELOCYPACK_UNLIKELY(slice.type(head) != ValueType::Object)) {
      throw Exception(Exception::InvalidValueType, "Expecting Object slice");
    }

    _size = slice.objectLength();

    if (_size > 0) {
      VELOCYPACK_ASSERT(head != 0x0a); // no empty object allowed here
      if (head == 0x14) {
        _current = slice.start() + slice.getStartOffsetFromCompact();
      } else if (useSequentialIteration) {
        _current = slice.begin() + slice.findDataOffset(head);
      }
      _first = _current;
    }
  }

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

  bool operator==(ObjectIterator const& other) const {
    return _position == other._position;
  }

  bool operator!=(ObjectIterator const& other) const {
    return !operator==(other);
  }

  ObjectPair operator*() const {
    if (_current != nullptr) {
      Slice key(_current);
      return ObjectPair(key.makeKey(), Slice(_current + key.byteSize()));
    }
    Slice key(_slice.getNthKeyUntranslated(_position));
    return ObjectPair(key.makeKey(), Slice(key.begin() + key.byteSize()));
  }
  
  ObjectIterator begin() const { 
    auto it = ObjectIterator(*this);
    it._position = 0;
    return it;
  }

  ObjectIterator end() const {
    auto it = ObjectIterator(*this);
    it._position = it._size;
    return it;
  }

  inline bool valid() const noexcept { return (_position < _size); }

  inline Slice key(bool translate = true) const {
    if (VELOCYPACK_UNLIKELY(_position >= _size)) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    if (_current != nullptr) {
      Slice s(_current);
      return translate ? s.makeKey() : s;
    }
    return _slice.getNthKey(_position, translate);
  }

  inline Slice value() const {
    if (VELOCYPACK_UNLIKELY(_position >= _size)) {
      throw Exception(Exception::IndexOutOfBounds);
    }
    if (_current != nullptr) {
      Slice key(_current);
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
  
  inline void reset() {
    _position = 0;
    _current = _first;
  }

 private:
  Slice _slice;
  ValueLength _size;
  ValueLength _position;
  uint8_t const* _current;
  uint8_t const* _first;
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
