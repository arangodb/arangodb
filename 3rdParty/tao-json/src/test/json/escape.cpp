// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/from_string.hpp>
#include <tao/json/to_string.hpp>

namespace tao::json
{
   void roundtrip( const std::string& v )
   {
      TEST_ASSERT( to_string( from_string( v ) ) == v );
   }

   void unit_test()
   {
      roundtrip( "\" \\u0000 \"" );
      roundtrip( "\" \\b \"" );
      roundtrip( "\" \\f \"" );
      roundtrip( "\" \\n \"" );
      roundtrip( "\" \\r \"" );
      roundtrip( "\" \\t \"" );
      roundtrip( "\" \\\\ \"" );
      roundtrip( "\" \\\" \"" );
      roundtrip( "\" \\u007f \"" );
      roundtrip( "\" \\b.\\f \"" );
      roundtrip( "\" \\b\\f\\n\\r\\t\\\\\\\" \"" );
      roundtrip( "\" \\bx\\fxz\\nzx\\ryx\\txy\\\\yz\\\" \"" );

      const std::string s1 = "\" \\b\\f\\n\\r\\t\\\\\\\" \"";
      TEST_ASSERT( from_string( s1 ).get_string() == " \b\f\n\r\t\\\" " );
      TEST_ASSERT( to_string( from_string( s1 ) ) == s1 );

      const std::string s2 = "\" \\u007f \"";
      TEST_ASSERT( from_string( s2 ).get_string() == " \x7f " );
      TEST_ASSERT( to_string( from_string( s2 ) ) == s2 );
   }

}  // namespace tao::json

#include "main.hpp"
