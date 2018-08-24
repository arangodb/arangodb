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

#ifndef IRESEARCH_UNICODE_UTILS_H
#define IRESEARCH_UNICODE_UTILS_H

NS_ROOT

const int32_t MIN_SUPPLEMENTARY_CODE_POINT = 65536;
const int32_t UNI_SUR_HIGH_START = 0xD800;
const int32_t HALF_SHIFT = 10;
const int32_t UNI_SUR_LOW_START = 0xDC00;
const int32_t SURROGATE_OFFSET = MIN_SUPPLEMENTARY_CODE_POINT - (UNI_SUR_HIGH_START << HALF_SHIFT) - UNI_SUR_LOW_START;

const uint32_t MAX_UTF8_BYTES_PER_CHAR = 4;

inline uint32_t utf8_size( uint32_t size ) {
  return size * MAX_UTF8_BYTES_PER_CHAR;
}

inline void char_to_wide( const char* src, size_t len, wchar_t* dst ) {
  for ( size_t i = 0; i < len; dst[i] = src[i], ++i );
}

uint32_t utf16_to_uft8(
  const wchar_t* s,
  uint32_t len,
  byte_type* out );

IRESEARCH_API bool utf8_less(
  const byte_type* lhs,
  size_t lhs_len,
  const byte_type* rhs,
  size_t rhs_len );

NS_END

#endif
