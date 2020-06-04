////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_RANGE_H
#define IRESEARCH_RANGE_H

#include "shared.hpp"

//////////////////////////////////////////////////////////////////////////////
/// @class range
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class range {
 public:
  typedef T value_type;

  constexpr range() noexcept: begin_(nullptr), end_(nullptr) {}
  constexpr range(value_type* data, size_t size) noexcept: begin_(data), end_(data + size) {}
  constexpr const value_type& operator[](size_t i) const noexcept {
    return IRS_ASSERT(i < size()), begin_[i];
  }
  constexpr bool empty() const noexcept { return begin_ == end_; }
  constexpr size_t size() const noexcept { return std::distance(begin_, end_); }
  constexpr value_type& operator[](size_t i) noexcept {
    return IRS_ASSERT(i < size()), begin_[i];
  }
  constexpr value_type* begin() noexcept { return begin_; }
  constexpr value_type* end() noexcept { return end_; }
  constexpr const value_type* begin() const noexcept { return begin_; }
  constexpr const value_type* end() const noexcept { return end_; }

 private:
  T* begin_;
  T* end_;
}; // range

#endif // IRESEARCH_RANGE_H
