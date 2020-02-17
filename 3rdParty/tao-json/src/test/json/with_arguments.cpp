// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json.hpp>

#include <tao/json/contrib/traits.hpp>

namespace tao::json
{
   struct arg_0
   {};

   struct type_1
   {
      template< template< typename... > class Traits >
      type_1( const basic_value< Traits >& /*unused*/, const arg_0& /*unused*/ )
      {}

      int i = 1;
   };

   void unit_test_1()
   {
      const arg_0 a;
      {
         const value v = 0;
         const auto b = v.as_with< std::shared_ptr< type_1 > >( a );
         TEST_ASSERT( b->i == 1 );
      }
      {
         const value v = 0;
         const auto b = v.as_with< std::shared_ptr< type_1 > >( arg_0() );
         TEST_ASSERT( b->i == 1 );
      }
      {
         const value v = from_string( "[ 1, 2, 3 ]" );
         const auto c = v.as_with< std::vector< std::shared_ptr< type_1 > > >( a );
         TEST_ASSERT( c.size() == 3 );
      }
   }

   struct type_2
   {
      int i = 2;
   };

   template<>
   struct traits< type_2 >
   {
      [[nodiscard]] static type_2 as( const value& /*unused*/, const arg_0& /*unused*/, const arg_0& /*unused*/ )
      {
         return type_2();
      }
   };

   void unit_test_2()
   {
      const arg_0 a;
      {
         const value v = from_string( "[ 1, 2, 3 ]" );
         const auto b = v.as_with< std::list< std::shared_ptr< type_2 > > >( a, a );
         TEST_ASSERT( b.size() == 3 );
      }
   }

   struct type_3
   {
      int i = 3;
   };

   template<>
   struct traits< type_3 >
   {
      static void to( const value& /*unused*/, type_3& /*unused*/, const arg_0& /*unused*/ ) {}
   };

   void unit_test_3()
   {
      const arg_0 a;
      {
         const value v = from_string( "[ 1, 2, 3 ]" );
         const auto b = v.as_with< std::deque< std::shared_ptr< type_3 > > >( a );
         TEST_ASSERT( b.size() == 3 );
      }
   }

   void unit_test()
   {
      unit_test_1();
      unit_test_2();
      unit_test_3();
   }

}  // namespace tao::json

#include "main.hpp"
