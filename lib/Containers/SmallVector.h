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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "Containers/details/short_alloc.h"

namespace arangodb {
namespace containers {

template <class T, std::size_t BufSize = 64, std::size_t ElementAlignment = alignof(T)>
using SmallVector = std::vector<T, detail::short_alloc<T, BufSize, ElementAlignment>>;

// helper class that combines a vector with a small-sized arena.
// objects of this class are supposed to be created on the stack and are
// neither copyable nor movable.
// the interface is a subset of std::vector's interface.
template <class T, std::size_t BufSize = 64, std::size_t ElementAlignment = alignof(T)>
class SmallVectorWithArena {
 public:
  SmallVectorWithArena() noexcept
      : _vector{_arena} {
    // reserve enough room in the arena to avoid early re-allocations later.
    // note that the initial allocation will take place in the arena, and not
    // use any extra heap memory.
    // later allocations can grow the vector beyond the size of the arena, and
    // then heap allocations will need to be made.
    _vector.reserve(BufSize / sizeof(T));
  }

  SmallVectorWithArena(SmallVectorWithArena const& other) = delete;
  SmallVectorWithArena& operator=(SmallVectorWithArena const& other) = delete;
  SmallVectorWithArena(SmallVectorWithArena&& other) = delete;
  SmallVectorWithArena& operator=(SmallVectorWithArena&& other) = delete;
  
  using AllocatorType = detail::short_alloc<T, BufSize, ElementAlignment>;
  using iterator = typename std::vector<T, AllocatorType>::iterator;
  using const_iterator = typename std::vector<T, AllocatorType>::const_iterator;
  using reverse_iterator = typename std::vector<T, AllocatorType>::reverse_iterator;
  using const_reverse_iterator = typename std::vector<T, AllocatorType>::const_reverse_iterator;

  void reserve(std::size_t n) {
    _vector.reserve(n);
  }
  
  void resize(std::size_t n) {
    _vector.resize(n);
  }

  void clear() {
    _vector.clear();
  }
  
  iterator insert(iterator pos, T const& value) {
    return _vector.insert(pos, value);
  }
  
  iterator insert(const_iterator pos, T const& value) {
    return _vector.insert(pos, value);
  }
  
  template <class... Args>
  void emplace_back(Args&&... args) {
    _vector.emplace_back(std::forward<Args>(args)...);
  }

  void pop_back() {
    _vector.pop_back();
  }
  
  void push_back(T const& value) {
    _vector.emplace_back(value);
  }

  std::size_t size() const noexcept {
    return _vector.size();
  }

  bool empty() const noexcept {
    return _vector.empty();
  }
  
  T const& operator[](std::size_t pos) const noexcept {
    return _vector[pos];
  }

  T& operator[](std::size_t pos) noexcept {
    return _vector[pos];
  }
  
  T const& at(std::size_t pos) const {
    return _vector.at(pos);
  }
  
  T& at(std::size_t pos) {
    return _vector.at(pos);
  }
  
  T const& front() const noexcept {
    return _vector.front();
  }

  T& front() noexcept {
    return _vector.front();
  }
  
  T const& back() const noexcept {
    return _vector.back();
  }

  T& back() noexcept {
    return _vector.back();
  }
  
  iterator begin() { return _vector.begin(); }
  iterator end() { return _vector.end(); }
  reverse_iterator rbegin() { return _vector.rbegin(); }
  reverse_iterator rend() { return _vector.rend(); }
  const_iterator begin() const { return _vector.begin(); }
  const_iterator end() const { return _vector.end(); }
  const_reverse_iterator rbegin() const { return _vector.rbegin(); }
  const_reverse_iterator rend() const { return _vector.rend(); }

  SmallVector<T, BufSize, ElementAlignment> const& vector() const noexcept {
    return _vector;
  }
  
  SmallVector<T, BufSize, ElementAlignment>& vector() noexcept {
    return _vector;
  }

 private:
  typename SmallVector<T, BufSize, ElementAlignment>::allocator_type::arena_type _arena;
  SmallVector<T, BufSize, ElementAlignment> _vector;
};

}  // namespace containers
}  // namespace arangodb

