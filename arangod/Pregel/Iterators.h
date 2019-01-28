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

namespace arangodb {
namespace pregel {

template <typename M>
class MessageIterator {
  M const* _data;
  size_t _current = 0;
  const size_t _size = 1;

 public:
  MessageIterator() : _data(nullptr), _current(0), _size(0) {}

  typedef MessageIterator<M> iterator;
  typedef const MessageIterator<M> const_iterator;

  explicit MessageIterator(M const* data) : _data(data), _size(data ? 1 : 0) {}
  explicit MessageIterator(M const* data, size_t s) : _data(data), _size(s) {}

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

  // M const& operator*() const { return *(_data + _current); }

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
  // void *_begin, *_end, *_current;
  T* _data;
  size_t _current, _size;

 public:
  typedef RangeIterator<T> iterator;
  typedef const RangeIterator<T> const_iterator;

  RangeIterator(T* v, size_t size) : _data(v), _current(0), _size(size) {}

  iterator begin() { return RangeIterator(_data, _size); }
  const_iterator begin() const { return RangeIterator(_data, _size); }
  iterator end() {
    auto it = RangeIterator(_data, _size);
    it._current = it._size;
    return it;
  }
  const_iterator end() const {
    auto it = RangeIterator(_data, _size);
    it._current = it._size;
    return it;
  }

  // prefix ++
  RangeIterator& operator++() {
    _current++;
    return *this;
  }

  // postfix ++
  RangeIterator<T>& operator++(int) {
    RangeIterator<T> result(*this);
    ++(*this);
    return result;
  }

  T* operator*() const { return _data + _current; }

  T* operator->() const { return _data + _current; }

  bool operator!=(RangeIterator<T> const& other) const {
    return _current != other._current;
  }

  size_t size() const { return _size; }

  /*EdgeIterator(void* beginPtr, void* endPtr)
   : _begin(beginPtr), _end(endPtr), _current(_begin) {}
   iterator begin() { return EdgeIterator(_begin, _end); }
   const_iterator begin() const { return EdgeIterator(_begin, _end); }
   iterator end() {
   auto it = EdgeIterator(_begin, _end);
   it._current = it._end;
   return it;
   }
   const_iterator end() const {
   auto it = EdgeIterator(_begin, _end);
   it._current = it._end;
   return it;
   }

   // prefix ++
   EdgeIterator<E>& operator++() {
   EdgeEntry<E>* entry = static_cast<EdgeEntry<E>>(_current);
   _current += entry->getSize();
   return *this;
   }

   EdgeEntry<E>* operator*() const {
   return _current != _end ? static_cast<EdgeEntry<E>>(_current) : nullptr;
   }*/
};
}  // namespace pregel
}  // namespace arangodb
#endif
