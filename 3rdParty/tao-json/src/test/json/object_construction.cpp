// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/to_string.hpp>
#include <tao/json/value.hpp>

// clang-format off

namespace tao::json
{
   struct X
   {};

   template<>
   struct traits< X >
   {
      TAO_JSON_DEFAULT_KEY( "X_key" );

      static void assign( value& v, const X& /*unused*/ )
      {
         v = "X_value";
      }
   };

   void unit_test()
   {
      X x;
      value j = 42;


      // recommended: top-level round brackets: value v( ... );
      {
         {
            // NON-UNIFORM: Does not call the default ctor
            // value v();  // most vexing parse, declares a function
         }
         // special case: nothing, work-around for the most vexing parse
         {
            value v;
            TEST_ASSERT( v.type() == type::UNINITIALIZED );
         }

         // create single values, not nested
         {
            value v( 0 );
            TEST_ASSERT( json::to_string( v ) == "0" );
         }
         {
            value v( x );
            TEST_ASSERT( json::to_string( v ) == "\"X_value\"" );
         }
         {
            value v( j );
            TEST_ASSERT( json::to_string( v ) == "42" );
         }

         // second-level curly brackets, uniform: create an object with members
#if ( __GNUC__ != 7 ) || ( __cplusplus < 201703L )
         {
            value v( {} );
            TEST_ASSERT( json::to_string( v ) == "{}" );
         }
#endif
         {
            value v( empty_object );
            TEST_ASSERT( json::to_string( v ) == "{}" );
         }
         {
            value v( { { "foo", 0 } } );
            TEST_ASSERT( json::to_string( v ) == "{\"foo\":0}" );
         }
         {
            value v( { x } );
            TEST_ASSERT( json::to_string( v ) == "{\"X_key\":\"X_value\"}" );
         }
         {
            value v( { x, { "foo", 0 } } );
            TEST_ASSERT( json::to_string( v ) == "{\"X_key\":\"X_value\",\"foo\":0}" );
         }
      }

      // recommended: top-level assignment: value v = ...;
      {
         // create single values, not nested
         {
            value v = 0;
            TEST_ASSERT( json::to_string( v ) == "0" );
         }
         {
            value v = x;
            TEST_ASSERT( json::to_string( v ) == "\"X_value\"" );
         }
         {
            value v = j;
            TEST_ASSERT( json::to_string( v ) == "42" );
         }

         // assign curly brackets
         {
            // NON-UNIFORM: Default ctor, does not create an empty object
            value v = {};
            TEST_ASSERT( v.type() == type::UNINITIALIZED );
         }
         {
            value v = empty_object;
            TEST_ASSERT( json::to_string( v ) == "{}" );
         }
         {
            value v = { { "foo", 0 } };
            TEST_ASSERT( json::to_string( v ) == "{\"foo\":0}" );
         }
         {
            value v = { x };
            TEST_ASSERT( json::to_string( v ) == "{\"X_key\":\"X_value\"}" );
         }
         {
            value v = { x, { "foo", 0 } };
            TEST_ASSERT( json::to_string( v ) == "{\"X_key\":\"X_value\",\"foo\":0}" );
         }
      }

      // NOT RECOMMENDED: top-level curly brackets: value v{ ... };
      {
         // create single values, not nested
         {
            // NON-UNIFORM: Default ctor, does not create an empty object
            value v{};
            TEST_ASSERT( v.type() == type::UNINITIALIZED );
         }
         {
            value v{ { "foo", 0 } };
            TEST_ASSERT( json::to_string( v ) == "{\"foo\":0}" );
         }
         {
            value v{ x };
            TEST_ASSERT( json::to_string( v ) == "{\"X_key\":\"X_value\"}" );
         }

         // second-level curly brackets, this doesn't make sense...
         {
            value v{ {} };
            TEST_ASSERT( json::to_string( v ) == "{}" );
         }
         {
            // doesn't even compile:
            // value v{ empty_object };
         }
         {
            value v{ { { "foo", 0 } } };
            TEST_ASSERT( json::to_string( v ) == "{\"foo\":0}" );
         }
         {
            value v{ { x } };
            TEST_ASSERT( json::to_string( v ) == "{\"X_key\":\"X_value\"}" );
         }
         {
            // doesn't even compile:
            // value v{ { x, { "foo", 0 } } };
         }
      }
   }

}  // namespace tao::json

#include "main.hpp"
