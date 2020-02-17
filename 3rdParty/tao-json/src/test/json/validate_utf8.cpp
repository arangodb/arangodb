// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/events/invalid_string_to_binary.hpp>
#include <tao/json/events/invalid_string_to_exception.hpp>
#include <tao/json/events/invalid_string_to_hex.hpp>
#include <tao/json/jaxn/to_string.hpp>
#include <tao/json/to_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   void test( const value& v, const std::string& b, const std::string& h )
   {
      TEST_ASSERT( jaxn::to_string< events::invalid_string_to_binary >( v ) == b );
      TEST_ASSERT( jaxn::to_string< events::invalid_string_to_hex >( v ) == h );
      TEST_THROWS( to_string< events::invalid_string_to_exception >( v ) );
   }

   void unit_test()
   {
      test( "\x80", "$80", "\"80\"" );
      test( "\xC0", "$C0", "\"C0\"" );
      test( "\xC0\x80", "$C080", "\"C080\"" );

      test( "A\xC0"
            "B\x80"
            "C",
            "$41C0428043",
            "\"41C0428043\"" );
   }

}  // namespace tao::json

#include "main.hpp"
