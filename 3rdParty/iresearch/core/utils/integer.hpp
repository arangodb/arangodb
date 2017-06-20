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

#ifndef IRESEARCH_INTEGER_H
#define IRESEARCH_INTEGER_H

#include "shared.hpp"

#include <limits>
#include <limits.h>

NS_BEGIN( detail )

template<class T, T min_val, T max_val>
struct integer_traits_base {
  static const bool is_integral = true;
  static const T const_min = min_val;
  static const T const_max = max_val;
};

NS_END

NS_ROOT

template< typename T >
struct integer_traits : std::numeric_limits < T > {
  static const bool is_integral = true;
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

NS_END

#endif
