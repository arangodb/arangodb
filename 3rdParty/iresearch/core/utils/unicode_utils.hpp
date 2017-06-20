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
