//
// IResearch search engine 
//
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
//
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
//

#ifndef IRESEARCH_TYPE_UTILS_H
#define IRESEARCH_TYPE_UTILS_H

#include "shared.hpp"
#include "std.hpp"
#include <type_traits>

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

NS_END

#endif