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

