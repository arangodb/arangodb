////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <cstddef>

namespace arangodb::pregel {

template<typename M>
class MessageIterator {
  M const* _data;
  const size_t _size;
  size_t _current;

 public:
  MessageIterator() : _data(nullptr), _size(0), _current(0) {}

  typedef MessageIterator<M> iterator;
  typedef const MessageIterator<M> const_iterator;

  explicit MessageIterator(M const* data)
      : _data(data), _size(data ? 1 : 0), _current(0) {}
  explicit MessageIterator(M const* data, size_t s)
      : _data(data), _size(s), _current(0) {}

  iterator begin() { return MessageIterator(_data, _size); }
  const_iterator begin() const { return MessageIterator(_data, _size); }
  iterator end() {
    auto it = MessageIterator(_data, _size);
    it._current = it._size;
    return it;
  }
  const_iterator end() const {
    auto it = MessageIterator(_data, _size);
    it._current = it._size;
    return it;
  }

  M const* operator*() const { return _data + _current; }

  M const* operator->() const { return _data + _current; }

  // prefix ++
  MessageIterator& operator++() {
    _current++;
    return *this;
  }

  // postfix ++
  MessageIterator operator++(int) {
    MessageIterator result(*this);
    ++(*this);
    return result;
  }

  bool operator!=(MessageIterator const& other) const {
    return _current != other._current;
  }

  size_t size() const { return _size; }
};

}  // namespace arangodb::pregel
