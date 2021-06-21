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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_INTEGER_H
#define IRESEARCH_INTEGER_H

#include "shared.hpp"

#include <limits>
#include <limits.h>

namespace detail {

template<class T, T min_val, T max_val>
struct integer_traits_base {
  static const T const_min = min_val;
  static const T const_max = max_val;
};

}

namespace iresearch {

template< typename T >
struct integer_traits : std::numeric_limits < T > {
};

template<> struct integer_traits< int8_t > :
  std::numeric_limits< int8_t >,
  ::detail::integer_traits_base < int8_t, INT8_MIN, INT8_MAX > {
};

template<> struct integer_traits< uint8_t > :
  std::numeric_limits< uint8_t >,
  ::detail::integer_traits_base < uint8_t, 0U, UINT8_MAX > {
};

template<> struct integer_traits< int16_t > :
  std::numeric_limits< int16_t >,
  ::detail::integer_traits_base < int16_t, INT16_MIN, INT16_MAX > {
};

template<> struct integer_traits< uint16_t > :
  std::numeric_limits< uint16_t >,
  ::detail::integer_traits_base < uint16_t, 0U, UINT16_MAX > {
};

template<> struct integer_traits< int32_t > :
  std::numeric_limits< int32_t >,
  ::detail::integer_traits_base < int32_t, INT32_MIN, INT32_MAX > {
};

template<> struct integer_traits< uint32_t > :
  std::numeric_limits< uint32_t >,
  ::detail::integer_traits_base < uint32_t, 0U, UINT32_MAX > {
};

template<> struct integer_traits< int64_t > :
  std::numeric_limits< int64_t >,
  ::detail::integer_traits_base < int64_t, INT64_MIN, INT64_MAX > {
};

template<> struct integer_traits< uint64_t > :
  std::numeric_limits< uint64_t >,
  ::detail::integer_traits_base < uint64_t, uint64_t(0), UINT64_MAX > {
};

// MacOS size_t is a different type from any of the above
#if defined(__APPLE__)
  template<> struct integer_traits<size_t>
    : std::numeric_limits<size_t>,
    ::detail::integer_traits_base <size_t, size_t(0), size_t(~0)> {
  };
#endif

}

#endif
