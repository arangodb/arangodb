// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/internal/uri_fragment.hpp>
#include <tao/json/pointer.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   void unit_test()
   {
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#" ) == ""_json_pointer );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/a~1b" ) == "/a~1b"_json_pointer );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/c%25d" ) == "/c%d"_json_pointer );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/c%d" ) );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/e%5Ef" ) == "/e^f"_json_pointer );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/e^f" ) );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/g%7Ch" ) == "/g|h"_json_pointer );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/g|h" ) );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/i%5Cj" ) == "/i\\j"_json_pointer );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/i\\j" ) );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/k%22l" ) == "/k\"l"_json_pointer );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/k\"l" ) );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/%20" ) == "/ "_json_pointer );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/ " ) );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/m~0n" ) == "/m~0n"_json_pointer );

      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/%30%31%32%33%34%35%36%37%38%39" ) == "/0123456789"_json_pointer );
      TEST_ASSERT( internal::uri_fragment_to_pointer( "#/%41%4A%4B%4C%4D%4E%4F" ) == "/AJKLMNO"_json_pointer );

      TEST_THROWS( internal::uri_fragment_to_pointer( "/" ) );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/%XY" ) );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#a" ) );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/~" ) );
      TEST_THROWS( internal::uri_fragment_to_pointer( "#/~2" ) );

      TEST_ASSERT( "#" == internal::to_uri_fragment( ""_json_pointer ) );
      TEST_ASSERT( "#/a~1b" == internal::to_uri_fragment( "/a~1b"_json_pointer ) );
      TEST_ASSERT( "#/c%25d" == internal::to_uri_fragment( "/c%d"_json_pointer ) );
      TEST_ASSERT( "#/e%5Ef" == internal::to_uri_fragment( "/e^f"_json_pointer ) );
      TEST_ASSERT( "#/g%7Ch" == internal::to_uri_fragment( "/g|h"_json_pointer ) );
      TEST_ASSERT( "#/i%5Cj" == internal::to_uri_fragment( "/i\\j"_json_pointer ) );
      TEST_ASSERT( "#/k%22l" == internal::to_uri_fragment( "/k\"l"_json_pointer ) );
      TEST_ASSERT( "#/%20" == internal::to_uri_fragment( "/ "_json_pointer ) );
      TEST_ASSERT( "#/m~0n" == internal::to_uri_fragment( "/m~0n"_json_pointer ) );
   }

}  // namespace tao::json

#include "main.hpp"
