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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef ARANGODB_CONTAINERS_ENUMERATE_H
#define ARANGODB_CONTAINERS_ENUMERATE_H
#include <functional>
#include <iterator>

namespace arangodb {

template <typename T, typename C>
struct enumerate_wrapper;

/*
 * enumerate function similar to pythons enumerate.
 * Usage:
 *  for (auto [idx, e] : enumerate(v)) {
 *    // idx is incremented for each element
 *    // e is the reference type of the iterator of the container v.
 *  }
 *
 * Usable for std::vector, std::list, std::map and std::unordered_map. _should_
 * work for other containers that provide a LegacyIterator as well.
 */
template <typename T, typename C = unsigned int>
enumerate_wrapper<T, C> enumerate(T&& v, C c = C());

template <typename T, typename C = unsigned int>
struct enumerate_iterator;

template <typename T, typename C>
void swap(enumerate_iterator<T, C>& a, enumerate_iterator<T, C>& b);

template <typename T, typename C>
struct enumerate_iterator {
  enumerate_iterator() : _iter(), _c(0) {}

  explicit enumerate_iterator(T iter, C c = C()) : _iter(iter), _c(c) {}
  enumerate_iterator(enumerate_iterator const&) = default;
  enumerate_iterator(enumerate_iterator&&) = default;

  enumerate_iterator& operator=(enumerate_iterator const&) = default;
  enumerate_iterator& operator=(enumerate_iterator&&) = default;

  ~enumerate_iterator() = default;

  friend void swap<>(enumerate_iterator<T, C>& a, enumerate_iterator<T, C>& b);

  using difference_type = typename std::iterator_traits<T>::difference_type;
  using value_type = std::pair<C, typename std::iterator_traits<T>::value_type>;
  using pointer = std::pair<C, typename std::iterator_traits<T>::pointer>;
  using reference = std::pair<C, typename std::iterator_traits<T>::reference>;
  using iterator_category = typename std::iterator_traits<T>::iterator_category;

  reference operator*() const;

  enumerate_iterator& operator++();

  bool operator==(enumerate_iterator const& other) const;
  bool operator!=(enumerate_iterator const& other) const;

  T _iter;
  C _c;
};

template <typename T, typename C>
auto enumerate_iterator<T, C>::operator*() const -> reference {
  return std::make_pair(_c, std::reference_wrapper(*_iter));
}

template <typename T, typename C>
struct enumerate_wrapper {
  using I = typename std::decay_t<T>::iterator;
  T _t;
  C _c;

  auto begin() { return enumerate_iterator(_t.begin(), _c); }
  auto end() { return enumerate_iterator(_t.end(), _c); }
};

template <typename T, typename C>
void swap(enumerate_iterator<T, C>& a, enumerate_iterator<T, C>& b) {
  using std::swap;
  swap(a._iter, b._iter);
  swap(a._c, b._c);
}

template <typename T, typename C>
auto enumerate_iterator<T, C>::operator++() -> enumerate_iterator& {
  ++_c;
  ++_iter;
  return *this;
}

template <typename T, typename C>
bool enumerate_iterator<T, C>::operator==(enumerate_iterator const& other) const {
  return _iter == other._iter;
}

template <typename T, typename C>
bool enumerate_iterator<T, C>::operator!=(enumerate_iterator const& other) const {
  return _iter != other._iter;
}

template <typename T, typename C>
enumerate_wrapper<T, C> enumerate(T&& v, C c) {
  return enumerate_wrapper<T, C>{std::forward<T>(v), c};
}
};  // namespace arangodb

#endif
