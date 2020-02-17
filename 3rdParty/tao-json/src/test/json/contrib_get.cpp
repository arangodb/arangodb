// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json.hpp>
#include <tao/json/contrib/get.hpp>

namespace tao::json
{
   void unit_test()
   {
      const value v = from_string( "{ \"a\": 1, \"b\": { \"ba\": 2, \"bb\": [ 3, 4, [], {}, { \"bb4a\": true } ] } }" );
      TEST_ASSERT( get::value( v, "a" ).is_value_ptr() );
      TEST_ASSERT( get::value( v, "a" ).skip_value_ptr().is_integer() );
      TEST_ASSERT( get::value( v, "a" ).skip_value_ptr().as< int >() == 1 );

      TEST_ASSERT( get::as< int >( get::value( v, "a" ) ) == 1 );
      TEST_ASSERT( !get::value( v, "zz" ) );
      TEST_THROWS( get::as< int >( get::value( v, "zz" ) ) );

      TEST_ASSERT( get::as< int >( v, "a" ) == 1 );
      TEST_THROWS( get::as< int >( v, "zz" ) );

      TEST_ASSERT( *get::optional< int >( v, "a" ) == 1 );
      TEST_ASSERT( !get::optional< int >( v, "zz" ) );
      TEST_ASSERT( !get::optional< int >( get::value( v, "zz" ) ) );

      TEST_ASSERT( get::defaulted( 2, v, "a" ) == 1 );
      TEST_ASSERT( get::defaulted( 2, v, "zz" ) == 2 );
      TEST_ASSERT( get::defaulted( 2, get::value( v, "zz" ) ) == 2 );

      TEST_ASSERT( get::as< bool >( v, "b", "bb", 4, "bb4a" ) == true );
      TEST_ASSERT( *get::optional< bool >( v, "b", "bb", 4, "bb4a" ) == true );
      TEST_ASSERT( get::defaulted( false, v, "b", "bb", 4, "bb4a" ) == true );

      TEST_ASSERT( !get::value( get::value( v, "zz" ), "zz" ) );
   }

}  // namespace tao::json

#include "main.hpp"
