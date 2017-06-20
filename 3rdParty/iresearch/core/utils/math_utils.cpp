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

#include "shared.hpp"
#include "math_utils.hpp"

NS_ROOT
NS_BEGIN( math )

uint32_t log2_64( uint64_t value ) {
  if ( 0 == value ) return 0;

  static const uint32_t tab64[64] = {
    63, 0, 58, 1, 59, 47, 53, 2,
    60, 39, 48, 27, 54, 33, 42, 3,
    61, 51, 37, 40, 49, 18, 28, 20,
    55, 30, 34, 11, 43, 14, 22, 4,
    62, 57, 46, 52, 38, 26, 32, 41,
    50, 36, 17, 19, 29, 10, 13, 21,
    56, 45, 25, 31, 35, 16, 9, 12,
    44, 24, 15, 8, 23, 7, 6, 5
  };

  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  value |= value >> 32;

  return tab64[( static_cast< uint64_t >( ( value - ( value >> 1 ) ) * 0x07EDD5E59A4E28C2 ) ) >> 58];
}

uint32_t log2_32( uint32_t value ) {
  static const uint32_t tab32[32] = {
    0, 9, 1, 10, 13, 21, 2, 29,
    11, 14, 16, 18, 22, 25, 3, 30,
    8, 12, 20, 28, 15, 17, 24, 7,
    19, 27, 23, 6, 26, 5, 4, 31
  };

  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;

  return tab32[static_cast< uint32_t >( value * 0x07C4ACDD ) >> 27];
}

uint32_t log(uint64_t x, uint64_t base) {
  assert(base > 1);

  uint32_t res = 0;
  while (x >= base) {
    ++res;
    x /= base;
  }

  return res;
}

NS_END // math
NS_END // root

