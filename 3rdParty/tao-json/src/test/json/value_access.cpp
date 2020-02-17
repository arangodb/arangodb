// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/from_string.hpp>
#include <tao/json/stream.hpp>
#include <tao/json/to_string.hpp>
#include <tao/json/value.hpp>

#include <tao/json/contrib/traits.hpp>

namespace tao::json
{
   struct one
   {
      int i;
   };

   struct two
   {
      int i;
   };

   struct both
   {
      int i;
   };

   template<>
   struct traits< one >
   {
      template< typename T >
      [[nodiscard]] static one as( const T& /*unused*/ )
      {
         return one{ 1 };
      }
   };

   template<>
   struct traits< two >
   {
      template< typename T >
      static void to( const T& /*unused*/, two& t )
      {
         t = two{ 2 };
      }
   };

   template<>
   struct traits< both >
   {
      template< typename T >
      [[nodiscard]] static both as( const T& /*unused*/ )
      {
         return both{ 3 };
      }

      template< typename T >
      static void to( const T& /*unused*/, both& t )
      {
         t = both{ 4 };
      }
   };

   void direct_test()
   {
      value v = 0;

      TEST_ASSERT( v.as< one >().i == 1 );
      {
         one i = { 0 };
         v.to( i );
         TEST_ASSERT( i.i == 1 );
      }
      TEST_ASSERT( v.as< two >().i == 2 );
      {
         two i = { 0 };
         v.to( i );
         TEST_ASSERT( i.i == 2 );
      }
      TEST_ASSERT( v.as< both >().i == 3 );
      {
         both i = { 0 };
         v.to( i );
         TEST_ASSERT( i.i == 4 );
      }
      TEST_ASSERT( v.optional< one >()->i == 1 );
      TEST_ASSERT( v.optional< two >()->i == 2 );
      TEST_ASSERT( v.optional< both >()->i == 3 );

      v = null;

      TEST_ASSERT( !v.optional< one >() );
      TEST_ASSERT( !v.optional< two >() );
      TEST_ASSERT( !v.optional< int >() );
      TEST_ASSERT( !v.optional< std::string >() );
      TEST_ASSERT( !v.optional< std::vector< std::string > >() );
   }

   void indirect_test()
   {
      value v = {
         { "foo", 0 }
      };

      TEST_ASSERT( v.as< one >( "foo" ).i == 1 );
      {
         one i = { 0 };
         v.to( i, "foo" );
         TEST_ASSERT( i.i == 1 );
      }
      TEST_ASSERT( v.as< two >( "foo" ).i == 2 );
      {
         two i = { 0 };
         v.to( i, "foo" );
         TEST_ASSERT( i.i == 2 );
      }
      TEST_ASSERT( v.as< both >( "foo" ).i == 3 );
      {
         both i = { 0 };
         v.to( i, "foo" );
         TEST_ASSERT( i.i == 4 );
      }
      TEST_ASSERT( v.optional< one >( "foo" )->i == 1 );
      TEST_ASSERT( v.optional< two >( "foo" )->i == 2 );
      TEST_ASSERT( v.optional< both >( "foo" )->i == 3 );

      TEST_ASSERT( !v.optional< one >( "bar" ) );
      TEST_ASSERT( !v.optional< two >( "bar" ) );
      TEST_ASSERT( !v.optional< int >( "bar" ) );
      TEST_ASSERT( !v.optional< std::string >( "bar" ) );
      TEST_ASSERT( !v.optional< std::vector< std::string > >( "bar" ) );
   }

   void unit_test()
   {
      direct_test();
      indirect_test();
   }

}  // namespace tao::json

#include "main.hpp"
