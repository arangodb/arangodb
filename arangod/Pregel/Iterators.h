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

#ifndef ARANGODB_PREGEL_ITERATOR_H
#define ARANGODB_PREGEL_ITERATOR_H 1

#include "Pregel/TypedBuffer.h"

namespace arangodb {
namespace pregel {

template <typename M>
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

template <typename T>
class RangeIterator {
 private:
  std::vector<std::unique_ptr<TypedBuffer<T>>>& _buffers;
  size_t _beginBuffer;
  T* _beginPtr;
  T* _currentBufferEnd;
  size_t _size;

 public:
  typedef RangeIterator<T> iterator;
  typedef const RangeIterator<T> const_iterator;

  RangeIterator(std::vector<std::unique_ptr<TypedBuffer<T>>>& bufs,
                size_t beginBuffer, T* beginPtr,
                size_t size) noexcept
    : _buffers(bufs),
      _beginBuffer(beginBuffer),
      _beginPtr(beginPtr),
      _currentBufferEnd(bufs.empty() ? beginPtr : bufs[_beginBuffer]->end()),
      _size(size) {}

  RangeIterator(RangeIterator const&) = delete;
  RangeIterator& operator=(RangeIterator const&) = delete;

  RangeIterator(RangeIterator&& other) noexcept
    : _buffers(other._buffers),
      _beginBuffer(other._beginBuffer),
      _beginPtr(other._beginPtr),
      _currentBufferEnd(other._currentBufferEnd),
      _size(other._size) {
    other._beginBuffer = 0;
    other._beginPtr = nullptr;
    other._currentBufferEnd = nullptr;
    other._size = 0;
  }

  RangeIterator& operator=(RangeIterator&& other) noexcept {
    TRI_ASSERT(&this->_buffers == &other._buffers);
    this->_beginBuffer = other._beginBuffer;
    this->_beginPtr = other._beginPtr;
    this->_currentBufferEnd = other._currentBufferEnd;
    this->_size = other._size;
    other._beginBuffer = 0;
    other._beginPtr = nullptr;
    other._currentBufferEnd = nullptr;
    other._size = 0;
    return *this;
  }

//  iterator begin() { return RangeIterator(_buffers.begin(), _begin, _end); }
//  const_iterator begin() const { return RangeIterator(_buffers.begin(), _begin, _end); }
  bool hasMore() const noexcept {
    return _size > 0;
  }

  // prefix ++
  RangeIterator& operator++() noexcept {
    TRI_ASSERT(_beginPtr != _currentBufferEnd);
    TRI_ASSERT(_size > 0);
    ++_beginPtr;
    --_size;
    if (_beginPtr == _currentBufferEnd && _size > 0) {
      ++_beginBuffer;
      TRI_ASSERT(_beginBuffer < _buffers.size());
      TypedBuffer<T>* tb = _buffers[_beginBuffer].get();
      _beginPtr = tb->begin();
      _currentBufferEnd = tb->end();
      TRI_ASSERT(_beginPtr != _currentBufferEnd);
    }
    return *this;
  }

  T* operator*() const { return _beginPtr; }

  T* operator->() const { return _beginPtr; }
};
}  // namespace pregel
}  // namespace arangodb
#endif
