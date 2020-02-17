// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/value.hpp>

#include <tao/json/contrib/patch.hpp>

namespace tao::json
{
   [[nodiscard]] value cpatch( const value& v, const value& patch )
   {
      return json::patch( v, patch );
   }

   void unit_test()
   {
      const value n = null;
      const value one = 1U;
      const value two = 2;
      const value ea = empty_array;
      const value eo = empty_object;

      const value a = { { "a", { { "foo", one } } } };
      const value b = { { "b", value::array( { one, two, 3, 4 } ) } };

      const value q = { { "q", { { "bar", two } } } };

      TEST_ASSERT( patch( n, ea ) == n );
      TEST_ASSERT( patch( ea, ea ) == ea );
      TEST_ASSERT( patch( eo, ea ) == eo );
      TEST_ASSERT( patch( a, ea ) == a );
      TEST_ASSERT( patch( b, ea ) == b );
      TEST_ASSERT( patch( q, ea ) == q );

      TEST_THROWS( patch( n, value::array( { { { "op", "foo" }, { "path", "" } } } ) ) );

      TEST_ASSERT( patch( n, value::array( { { { "op", "test" }, { "path", "" }, { "value", null } } } ) ) == null );
      TEST_THROWS( patch( n, value::array( { { { "op", "test" }, { "path", "" }, { "value", 42 } } } ) ) );

      TEST_ASSERT( patch( a, value::array( { { { "op", "test" }, { "path", "/a" }, { "value", { { "foo", 1 } } } } } ) ) == a );
      TEST_ASSERT( patch( a, value::array( { { { "op", "test" }, { "path", "/a/foo" }, { "value", 1 } } } ) ) == a );

      TEST_THROWS( patch( a, value::array( { { { "op", "test" }, { "path", "" }, { "value", 42 } } } ) ) );
      TEST_THROWS( patch( a, value::array( { { { "op", "test" }, { "path", "/a" }, { "value", 42 } } } ) ) );

      TEST_ASSERT( cpatch( n, ea ) == n );
      TEST_ASSERT( cpatch( ea, ea ) == ea );
      TEST_ASSERT( cpatch( eo, ea ) == eo );
      TEST_ASSERT( cpatch( a, ea ) == a );
      TEST_ASSERT( cpatch( b, ea ) == b );
      TEST_ASSERT( cpatch( q, ea ) == q );

      TEST_THROWS( cpatch( n, value::array( { { { "op", "foo" }, { "path", "" } } } ) ) );

      TEST_ASSERT( cpatch( n, value::array( { { { "op", "test" }, { "path", "" }, { "value", null } } } ) ) == null );
      TEST_THROWS( cpatch( n, value::array( { { { "op", "test" }, { "path", "" }, { "value", 42 } } } ) ) );

      TEST_ASSERT( cpatch( a, value::array( { { { "op", "test" }, { "path", "/a" }, { "value", { { "foo", 1 } } } } } ) ) == a );
      TEST_ASSERT( cpatch( a, value::array( { { { "op", "test" }, { "path", "/a/foo" }, { "value", 1 } } } ) ) == a );

      TEST_THROWS( cpatch( a, value::array( { { { "op", "test" }, { "path", "" }, { "value", 42 } } } ) ) );
      TEST_THROWS( cpatch( a, value::array( { { { "op", "test" }, { "path", "/a" }, { "value", 42 } } } ) ) );
   }

}  // namespace tao::json

#include "main.hpp"
