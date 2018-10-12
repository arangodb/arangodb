////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_TYPE_UTILS_H
#define IRESEARCH_TYPE_UTILS_H

#include "shared.hpp"
#include "std.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                             template type traits
// ----------------------------------------------------------------------------

template <typename... Types>
struct template_traits_t;

template<typename First, typename... Second>
struct template_traits_t<First, Second...> {
  static CONSTEXPR size_t count() NOEXCEPT {
    return 1 + template_traits_t<Second...>::count();
  }

  static CONSTEXPR size_t size() NOEXCEPT {
    return sizeof(First) + template_traits_t<Second...>::size();
  }

  static CONSTEXPR size_t size_aligned(size_t start = 0) NOEXCEPT {
    return template_traits_t<Second...>::size_aligned(
      template_traits_t<First>::size_max_aligned(start)
    );
  }

  static CONSTEXPR size_t align_max(size_t max = 0) NOEXCEPT {
    return template_traits_t<Second...>::align_max(
      irstd::max(max, std::alignment_of<First>::value)
    );
  }

  static CONSTEXPR size_t size_max(size_t max = 0) NOEXCEPT {
    return template_traits_t<Second...>::size_max(
      irstd::max(max, sizeof(First))
    );
  }

  static CONSTEXPR size_t offset_aligned(size_t start = 0) NOEXCEPT {
    typedef std::alignment_of<First> align_t;

    return start
      + ((align_t::value - (start % align_t::value)) % align_t::value) // padding
      + sizeof(First);
  }

  static CONSTEXPR size_t size_max_aligned(size_t start = 0, size_t max = 0) NOEXCEPT {
    return template_traits_t<Second...>::size_max_aligned(
      start, (irstd::max)(max, offset_aligned(start))
    );
  }

  template <typename T>
  static CONSTEXPR bool is_convertible() NOEXCEPT {
    return std::is_convertible<First, T>::value
      || template_traits_t<Second...>::template is_convertible<T>();
  }

  template<typename T>
  static CONSTEXPR bool in_list() NOEXCEPT {
    return std::is_same<First, T>::value
      || template_traits_t<Second...>::template in_list<T>();
  }
}; // template_traits_t

template<>
struct template_traits_t<> {
  static CONSTEXPR size_t count() NOEXCEPT {
    return 0;
  }

  static CONSTEXPR size_t size() NOEXCEPT {
    return 0;
  }

  static CONSTEXPR size_t size_aligned(size_t start = 0) NOEXCEPT {
    return start;
  }

  static CONSTEXPR size_t align_max(size_t max = 0) NOEXCEPT {
    return max;
  }

  static CONSTEXPR size_t size_max(size_t max = 0) NOEXCEPT {
    return max;
  }

  static CONSTEXPR size_t offset_aligned(size_t start = 0) NOEXCEPT {
    return start;
  }

  static CONSTEXPR size_t size_max_aligned(size_t start = 0, size_t max = 0) NOEXCEPT {
    return max ? max : start;
  }

  template<typename T>
  static CONSTEXPR bool is_convertible() NOEXCEPT {
    return false;
  }

  template<typename T>
  static CONSTEXPR bool in_list() NOEXCEPT {
    return false;
  }
}; // template_traits_t

///////////////////////////////////////////////////////////////////////////////
/// @returns true if type 'T' is convertible to one of the specified 'Types',
///          false otherwise
///////////////////////////////////////////////////////////////////////////////
template<typename T, typename... Types>
CONSTEXPR bool is_convertible() NOEXCEPT {
  return template_traits_t<Types...>::template is_convertible<T>();
}

///////////////////////////////////////////////////////////////////////////////
/// @returns true if type 'T' is present in the specified list of 'Types',
///          false otherwise
///////////////////////////////////////////////////////////////////////////////
template<typename T, typename... Types>
CONSTEXPR bool in_list() NOEXCEPT {
  return template_traits_t<Types...>::template in_list<T>();
}

///////////////////////////////////////////////////////////////////////////////
/// @returns if 'T' is 'std::shared_ptr<T>' provides the member constant value
///          equal to 'true', or 'false' otherwise
///////////////////////////////////////////////////////////////////////////////
template<typename T>
struct is_shared_ptr : std::false_type {};

template<typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

NS_END

#endif