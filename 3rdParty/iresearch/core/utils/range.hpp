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
/// @author Vasiliy Nabatchikov
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

  CONSTEXPR range() NOEXCEPT: begin_(nullptr), end_(nullptr) {}
  CONSTEXPR range(value_type* data, size_t size) NOEXCEPT: begin_(data), end_(data + size) {}
  CONSTEXPR const value_type& operator[](size_t i) const NOEXCEPT {
    return IRS_ASSERT(i < size()), begin_[i];
  }
  CONSTEXPR bool empty() const NOEXCEPT { return begin_ == end_; }
  CONSTEXPR size_t size() const NOEXCEPT { return std::distance(begin_, end_); }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  CONSTEXPR value_type& operator[](size_t i) NOEXCEPT {
    return IRS_ASSERT(i < size()), begin_[i];
  }
  CONSTEXPR value_type* begin() NOEXCEPT { return begin_; }
  CONSTEXPR value_type* end() NOEXCEPT { return end_; }
#else
  // before c++14 constexpr member function
  // gets const implicitly
  value_type& operator[](size_t i) NOEXCEPT {
    return IRS_ASSERT(i < size()), begin_[i];
  }
  value_type* begin() NOEXCEPT { return begin_; }
  value_type* end() NOEXCEPT { return end_; }
#endif
  CONSTEXPR const value_type* begin() const NOEXCEPT { return begin_; }
  CONSTEXPR const value_type* end() const NOEXCEPT { return end_; }

 private:
  T* begin_;
  T* end_;
}; // range

#endif // IRESEARCH_RANGE_H
