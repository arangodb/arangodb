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
#include "unicode_utils.hpp"

#include <algorithm>

NS_ROOT

bool utf8_less( const byte_type* lhs, size_t lhs_len, 
                const byte_type* rhs, size_t rhs_len ) {
  if (lhs == rhs) {
    return lhs_len < rhs_len;
  }

  const size_t len = std::min( lhs_len, rhs_len );

  for ( size_t i = 0; i < len; ++i ) {
    const byte_type lhs_b = *lhs; ++lhs;
    const byte_type rhs_b = *rhs; ++rhs;

    if ( lhs_b < rhs_b ) {
      return true;
    }

    if ( lhs_b > rhs_b ) {
      return false;
    }
  }

  return lhs_len < rhs_len;
}

uint32_t utf16_to_uft8( const wchar_t* s, uint32_t len, byte_type* out ) {
  uint32_t pos = 0;
  for ( uint32_t i = 0; i < len; ++i ) {
    const int32_t code = static_cast< int32_t >( s[i] );
    if ( code < 0x80 ) {
      out[pos++] = static_cast< byte_type >( code );

    } else if ( code < 0x800 ) {
      out[pos++] = static_cast< byte_type >( 0xC0 | ( code >> 6 ) );
      out[pos++] = static_cast< byte_type >( 0x80 | ( code & 0x3f ) );
    } else if ( code < 0xD800 || code > 0xDFFF ) {
      out[pos++] = static_cast< byte_type >( 0xE0 | ( code >> 12 ) );
      out[pos++] = static_cast< byte_type >( 0x80 | ( ( code >> 6 ) & 0x3F ) );
      out[pos++] = static_cast< byte_type >( 0x80 | ( code & 0x3F ) );
    } else {
      // surrogate pair
      // confirm valid high surrogate
      if ( code < 0xDC00 && i < len ) {
        int32_t utf32 = static_cast< int32_t >( s[i] );
        // confirm valid low surrogate and write pair
        if ( utf32 >= 0xDC00 && utf32 <= 0xDFFF ) {
          utf32 = ( code << 10 ) + utf32 + SURROGATE_OFFSET;
          i++;
          out[pos++] = static_cast< byte_type >( 0xF0 | ( utf32 >> 18 ) );
          out[pos++] = static_cast< byte_type >( 0x80 | ( ( utf32 >> 12 ) & 0x3F ) );
          out[pos++] = static_cast< byte_type >( 0x80 | ( ( utf32 >> 6 ) & 0x3F ) );
          out[pos++] = static_cast< byte_type >( 0x80 | ( utf32 & 0x3F ) );
          continue;
        }
      }
      // replace unpaired surrogate or out-of-order low surrogate
      // with substitution character
      out[pos++] = static_cast< byte_type >( 0xEF );
      out[pos++] = static_cast< byte_type >( 0xBF );
      out[pos++] = static_cast< byte_type >( 0xBD );
    }
  }

  return pos;
}

NS_END

