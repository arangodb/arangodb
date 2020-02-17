// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/internal/sha256.hpp>

namespace tao::json
{
   static char hex[] = "0123456789ABCDEF";

   [[nodiscard]] std::string to_hex( const std::string& s )
   {
      std::string result;
      result.reserve( s.size() * 2 );
      for( unsigned char c : s ) {
         result += hex[ c >> 4 ];
         result += hex[ c & 15 ];
      }
      return result;
   }

   void unit_test()
   {
      // two test vectors from RFC 6234 should be enough

      internal::sha256 d;
      d.feed( "abc" );
      TEST_ASSERT( to_hex( d.get() ) == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD" );

      d.reset();
      d.feed( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" );
      TEST_ASSERT( to_hex( d.get() ) == "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1" );
   }

}  // namespace tao::json

#include "main.hpp"
