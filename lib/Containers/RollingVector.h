////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CONTAINER_ROLLING_VECTOR_H
#define ARANGODB_CONTAINER_ROLLING_VECTOR_H 1

#include <type_traits>

#include "Basics/debugging.h"

namespace arangodb {
namespace containers {

/// @brief a vector wrapper that also provides amortized O(1) pop_front()
/// functionality. pop_front() is implemented by adjusting a start index into
/// the vector, which is initially zero. every call to pop_front() will only
/// increase the start index by one, which is very efficient. the efficiency is
/// "bought" by not reclaiming unused space for popped front elements. note that
/// the elements popped with pop_front() will also not be destructed when
/// popped. this means this container can only be used for managing trivial
/// types (e.g. integers or pointers) that do not require ad-hoc destruction
template <typename T>
class RollingVector {
 public:
  RollingVector() : _start(0) {}
  explicit RollingVector(size_t size) : RollingVector() { _data.resize(size); }

  RollingVector(RollingVector const& other)
      : _start(other._start), _data(other._data) {}
  RollingVector& operator=(RollingVector const& other) {
    if (this != &other) {
      _start = other._start;
      _data = other._data;
    }
    return *this;
  }

  RollingVector(RollingVector&& other)
      : _start(other._start), _data(std::move(other._data)) {
    other.clear();
  }

  RollingVector& operator=(RollingVector&& other) {
    if (this != &other) {
      _start = other._start;
      _data = std::move(other._data);
      other.clear();
    }
    return *this;
  }

  ~RollingVector() = default;

  typename std::vector<T>::iterator begin() { return _data.begin() + _start; }

  typename std::vector<T>::iterator end() { return _data.end(); }

  typename std::vector<T>::const_iterator begin() const {
    return _data.begin();
  }

  typename std::vector<T>::const_iterator end() const { return _data.end(); }

  T& operator[](size_t position) { return _data[_start + position]; }

  T const& operator[](size_t position) const {
    return _data[_start + position];
  }

  T& at(size_t position) { return _data.at(_start + position); }

  T const& at(size_t position) const { return _data.at(_start + position); }

  void reserve(size_t size) { _data.reserve(_start + size); }

  void push_back(T const& value) { _data.push_back(value); }

  void push_back(T&& value) { _data.push_back(std::move(value)); }

  void pop_front() {
    TRI_ASSERT(!empty());
    ++_start;
    if (_start == _data.size()) {
      // use the opportunity to reset the start value
      clear();
    }
  }

  void pop_back() {
    TRI_ASSERT(!empty());
    _data.pop_back();
    if (_start == _data.size()) {
      // use the opportunity to reset the start value
      clear();
    }
  }

  T const& front() const {
    TRI_ASSERT(!empty());
    return _data[_start];
  }

  T& front() { return _data[_start]; }

  T const& back() const {
    TRI_ASSERT(!empty());
    return _data.back();
  }

  T& back() {
    TRI_ASSERT(!empty());
    return _data.back();
  }

  bool empty() const { return (_start >= _data.size()); }

  size_t size() const { return _data.size() - _start; }

  void clear() {
    _data.clear();
    _start = 0;
  }

  void shrink_to_fit() { _data.shrink_to_fit(); }

  void swap(RollingVector& other) {
    std::swap(_data, other._data);
    std::swap(_start, other._start);
  }

 private:
  size_t _start;
  std::vector<T> _data;
};

}  // namespace containers
}  // namespace arangodb

#endif
