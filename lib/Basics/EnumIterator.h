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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ENUM_ITERATOR_H
#define ARANGODB_BASICS_ENUM_ITERATOR_H 1

#include "Basics/Common.h"

#include <type_traits>

#define ENUM_ITERATOR(type, start, end) \
  arangodb::EnumIterator<type, type::start, type::end>()

namespace arangodb {

/// @brief Iterator for an enum class type
/// will work only when the enum values are unique, contiguous and
/// sorted in order
template <typename T, T beginValue, T endValue>
class EnumIterator {
  typedef typename std::underlying_type<T>::type ValueType;

 public:
  explicit EnumIterator(T const& current) noexcept
      : current(static_cast<ValueType>(current)) {}

  EnumIterator() : current(static_cast<ValueType>(beginValue)) {}

  EnumIterator operator++() noexcept {
    ++current;
    return *this;
  }

  T operator*() noexcept { return static_cast<T>(current); }

  EnumIterator begin() noexcept { return *this; }
  EnumIterator end() noexcept {
    static const EnumIterator endIter = ++EnumIterator(endValue);
    return endIter;
  }

  bool operator!=(EnumIterator const& other) const noexcept {
    return current != other.current;
  }

 private:
  int current;
};

}  // namespace arangodb

#endif
