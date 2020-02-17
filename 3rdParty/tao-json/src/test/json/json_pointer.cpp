// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/pointer.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   void unit_test()
   {
      // do not throw
      (void)"//"_json_pointer;
      (void)"/-"_json_pointer;
      (void)"/99999999999999999999999999999999999999"_json_pointer;
      (void)"/99999999999999999999"_json_pointer;

      // throw
      TEST_THROWS( "x"_json_pointer );
      TEST_THROWS( "/~"_json_pointer );
      TEST_THROWS( "/~2"_json_pointer );
      TEST_THROWS( "/~x"_json_pointer );

      // create test vector
      const std::string ezp( "o\0p", 3 );
      const std::string ezq( "o\0q", 3 );
      value v{
         { "foo", value::array( { "bar", "baz" } ) },
         { "", 0 },
         { "a/b", 1 },
         { "c%d", 2 },
         { "e^f", 3 },
         { "g|h", 4 },
         { "i\\j", 5 },
         { "k\"l", 6 },
         { " ", 7 },
         { "m~n", 8 },
         { ezp, 9 },
         { ezq, 10 },
         { "-", 11 },
         { "007", 12 }
      };

      // check access
      TEST_ASSERT( v.at( ""_json_pointer ) == v );
      TEST_ASSERT( *v.find( ""_json_pointer ) == v );

      TEST_ASSERT( v.at( "/foo"_json_pointer ) == value::array( { "bar", "baz" } ) );
      TEST_ASSERT( *v.find( "/foo"_json_pointer ) == value::array( { "bar", "baz" } ) );
      TEST_THROWS( v.at( "/bar"_json_pointer ) );
      TEST_ASSERT( v.find( "/bar"_json_pointer ) == nullptr );
      TEST_THROWS( v.erase( "/foo/0/baz"_json_pointer ) );
      TEST_THROWS( v.insert( "/foo/0/baz"_json_pointer, null ) );

      TEST_ASSERT( v.at( "/foo/0"_json_pointer ) == "bar" );
      TEST_ASSERT( *v.find( "/foo/0"_json_pointer ) == "bar" );
      TEST_ASSERT( v.at( "/foo/1"_json_pointer ) == "baz" );
      TEST_ASSERT( *v.find( "/foo/1"_json_pointer ) == "baz" );
      TEST_THROWS( v.at( "/foo/2"_json_pointer ) );
      TEST_ASSERT( v.find( "/foo/2"_json_pointer ) == nullptr );
      TEST_THROWS( v.find( "/ /2"_json_pointer ) );

      TEST_THROWS( v.at( "/foo/0/bar"_json_pointer ) );
      TEST_THROWS( v.find( "/foo/0/bar"_json_pointer ) );
      TEST_THROWS( v.at( "/foo/00"_json_pointer ) );
      TEST_THROWS( v.at( "/foo/01"_json_pointer ) );
      TEST_THROWS( v.at( "/foo/0 "_json_pointer ) );
      TEST_THROWS( v.at( "/foo/ 0"_json_pointer ) );
      TEST_THROWS( v.at( "/foo/1 "_json_pointer ) );
      TEST_THROWS( v.at( "/foo/ 1"_json_pointer ) );
      TEST_THROWS( v.at( "/foo/-"_json_pointer ) );
      TEST_THROWS( v.at( "/foo/bar"_json_pointer ) );

      TEST_ASSERT( v.at( "/"_json_pointer ) == 0 );
      TEST_THROWS( v.at( "//"_json_pointer ) );

      TEST_ASSERT( v.at( "/a~1b"_json_pointer ) == 1 );
      TEST_ASSERT( v.at( "/c%d"_json_pointer ) == 2 );
      TEST_ASSERT( v.at( "/e^f"_json_pointer ) == 3 );
      TEST_ASSERT( v.at( "/g|h"_json_pointer ) == 4 );
      TEST_ASSERT( v.at( "/i\\j"_json_pointer ) == 5 );
      TEST_ASSERT( v.at( "/k\"l"_json_pointer ) == 6 );
      TEST_ASSERT( v.at( "/ "_json_pointer ) == 7 );
      TEST_ASSERT( v.at( "/m~0n"_json_pointer ) == 8 );
      TEST_ASSERT( v.at( "/o\0p"_json_pointer ) == 9 );
      TEST_ASSERT( v.at( "/o\0q"_json_pointer ) == 10 );
      TEST_ASSERT( v.at( "/-"_json_pointer ) == 11 );
      TEST_ASSERT( v.at( "/007"_json_pointer ) == 12 );

      // check modifications
      v[ "/foo/-"_json_pointer ] = "bat";
      TEST_ASSERT( v.at( "/foo"_json_pointer ) != value::array( { "bar", "baz" } ) );
      TEST_ASSERT( v.at( "/foo"_json_pointer ) == value::array( { "bar", "baz", "bat" } ) );

      (void)v[ "/foo/-"_json_pointer ];  // no assignment, but the null is appended anyways... TODO: change that to require an assignment?
      TEST_ASSERT( v.at( "/foo"_json_pointer ) != value::array( { "bar", "baz", "bat" } ) );
      TEST_ASSERT( v.at( "/foo"_json_pointer ) == value::array( { "bar", "baz", "bat", null } ) );

      TEST_THROWS( v.at( "bar" ) );
      v[ "/bar"_json_pointer ] = 42;
      TEST_ASSERT( v.at( "bar" ) == 42 );

      TEST_ASSERT( v[ ""_json_pointer ] == v );
      TEST_ASSERT( v[ "/foo/0"_json_pointer ] == "bar" );
      TEST_ASSERT( v[ "/foo/1"_json_pointer ] == "baz" );
      TEST_THROWS( v[ "/foo/4"_json_pointer ] );

      TEST_THROWS( v[ "/foo/0/bar"_json_pointer ] );
      TEST_THROWS( v[ "/foo/00"_json_pointer ] );
      TEST_THROWS( v[ "/foo/01"_json_pointer ] );
      TEST_THROWS( v[ "/foo/0 "_json_pointer ] );
      TEST_THROWS( v[ "/foo/ 0"_json_pointer ] );
      TEST_THROWS( v[ "/foo/1 "_json_pointer ] );
      TEST_THROWS( v[ "/foo/ 1"_json_pointer ] );
      TEST_THROWS( v[ "/foo/bar"_json_pointer ] );
      TEST_THROWS( v[ "/foo/-/bar"_json_pointer ] );

      TEST_ASSERT( v[ "/a~1b"_json_pointer ] == 1 );
      TEST_ASSERT( v[ "/c%d"_json_pointer ] == 2 );
      TEST_ASSERT( v[ "/e^f"_json_pointer ] == 3 );
      TEST_ASSERT( v[ "/g|h"_json_pointer ] == 4 );
      TEST_ASSERT( v[ "/i\\j"_json_pointer ] == 5 );
      TEST_ASSERT( v[ "/k\"l"_json_pointer ] == 6 );
      TEST_ASSERT( v[ "/ "_json_pointer ] == 7 );
      TEST_ASSERT( v[ "/m~0n"_json_pointer ] == 8 );
      TEST_ASSERT( v[ "/o\0p"_json_pointer ] == 9 );
      TEST_ASSERT( v[ "/o\0q"_json_pointer ] == 10 );
      TEST_ASSERT( v[ "/-"_json_pointer ] == 11 );
      TEST_ASSERT( v[ "/007"_json_pointer ] == 12 );

      TEST_THROWS( v.at( "/o\0r"_json_pointer ) );
      TEST_ASSERT( v[ "/o\0r"_json_pointer ].is_uninitialized() );
      TEST_ASSERT( v.at( "/o\0r"_json_pointer ).is_uninitialized() );

      const value v2( { { "x~y/z", 0 } } );
      TEST_ASSERT( v2.at( "/x~0y~1z"_json_pointer ) == 0 );
      TEST_THROWS( v2.at( "/x~0y~1z/0"_json_pointer ) );

      const auto p1 = ""_json_pointer;
      const auto p2 = "/a"_json_pointer;
      const auto p3 = "/b"_json_pointer;
      const auto p4 = p2;

      TEST_ASSERT( !( p1 == p2 ) );
      TEST_ASSERT( p1 != p2 );
      TEST_ASSERT( p1 < p2 );
      TEST_ASSERT( !( p1 > p2 ) );
      TEST_ASSERT( p1 <= p2 );
      TEST_ASSERT( !( p1 >= p2 ) );

      TEST_ASSERT( !( p1 == p3 ) );
      TEST_ASSERT( p1 != p3 );
      TEST_ASSERT( p1 < p3 );
      TEST_ASSERT( !( p1 > p3 ) );
      TEST_ASSERT( p1 <= p3 );
      TEST_ASSERT( !( p1 >= p3 ) );

      TEST_ASSERT( !( p2 == p3 ) );
      TEST_ASSERT( p2 != p3 );
      TEST_ASSERT( p2 < p3 );
      TEST_ASSERT( !( p2 > p3 ) );
      TEST_ASSERT( p2 <= p3 );
      TEST_ASSERT( !( p2 >= p3 ) );

      TEST_ASSERT( !( p4 == p3 ) );
      TEST_ASSERT( p4 != p3 );
      TEST_ASSERT( p4 < p3 );
      TEST_ASSERT( !( p4 > p3 ) );
      TEST_ASSERT( p4 <= p3 );
      TEST_ASSERT( !( p4 >= p3 ) );
   }

}  // namespace tao::json

#include "main.hpp"
