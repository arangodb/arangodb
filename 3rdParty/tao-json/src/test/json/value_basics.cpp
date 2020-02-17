// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <algorithm>

#include "test.hpp"
#include "test_types.hpp"

#include <tao/json/value.hpp>

namespace tao::json
{
   static unsigned counter = 0;

   struct base
   {
      enum state
      {
         destroyed,
         copy_assigned,
         copy_assigned_from,
         move_assigned,
         move_assigned_from,
         copy_constructed,
         copy_constructed_from,
         move_constructed,
         move_constructed_from,
         default_constructed
      };

      unsigned number;
      mutable state s;

      base() noexcept
         : number( ++counter ),
           s( default_constructed )
      {
      }

      base( base&& b ) noexcept
         : number( b.number ),
           s( move_constructed )
      {
         b.s = move_constructed_from;
         b.number = 0;
      }

      base( const base& b ) noexcept
         : number( b.number ),
           s( copy_constructed )
      {
         b.s = copy_constructed_from;
      }

      void operator=( base&& b ) noexcept
      {
         s = move_assigned;
         b.s = move_assigned_from;
         number = b.number;
         b.number = 0;
      }

      void operator=( const base& b ) noexcept
      {
         s = copy_assigned;
         b.s = copy_assigned_from;
         number = b.number;
      }

      ~base()
      {
         s = destroyed;
         number = 0;
      }
   };

   template< typename T >
   struct base_traits
      : traits< T >
   {
   };

   template<>
   struct base_traits< void >
      : traits< void >
   {
      template< typename >
      using public_base = base;
   };

   using based = basic_value< base_traits >;

   void unit_test()
   {
      {
         value v;
         assert_uninitialized( v );
         v.set_null();
         assert_null( v );
         v.set_uninitialized();
         assert_uninitialized( v );
         v.set_boolean( true );
         assert_boolean( v, true );
         v.set_signed( -1 );
         assert_signed( v, -1 );
         v.set_unsigned( 2 );
         assert_unsigned( v, 2 );
         v.set_double( 42.0 );
         assert_double( v, 42.0 );
         v.set_boolean( false );
         assert_boolean( v, false );
      }
      {
         value v = 42;
         TEST_ASSERT( v == 42 );
         value w = std::move( v );
         TEST_ASSERT( w == 42 );
      }
      {
         value v = 42;
         TEST_ASSERT( v == 42 );
         value w = v;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( w == 42 );
      }
      {
         value v = 42;
         value w = true;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( w == true );
         v = w;
         TEST_ASSERT( v == true );
         TEST_ASSERT( w == true );
      }
      {
         value v = 42;
         value w = true;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( w == true );
         v = std::move( w );
         TEST_ASSERT( v == true );
      }
      {
         value v = 42;
         value w = true;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( w == true );
         std::swap( v, w );
         TEST_ASSERT( v == true );
         TEST_ASSERT( w == 42 );
      }
      {
         counter = 0;
         based v;
         assert_uninitialized( v );
         TEST_ASSERT( v.public_base().s == base::default_constructed );
         TEST_ASSERT( v.public_base().number == 1 );
      }
      {
         counter = 0;
         based v = 42;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( v.public_base().s == base::move_constructed );  // From defaulted second argument to basic_value::basic_value().
         TEST_ASSERT( v.public_base().number == 1 );
         based w = std::move( v );
         TEST_ASSERT( w == 42 );
         TEST_ASSERT( v.public_base().s == base::move_constructed_from );
         TEST_ASSERT( w.public_base().s == base::move_constructed );
         TEST_ASSERT( v.public_base().number == 0 );
         TEST_ASSERT( w.public_base().number == 1 );
      }
      {
         counter = 0;
         based v = 42;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( v.public_base().s == base::move_constructed );  // From defaulted second argument to basic_value::basic_value().
         TEST_ASSERT( v.public_base().number == 1 );
         based w = v;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( w == 42 );
         TEST_ASSERT( v.public_base().s == base::copy_constructed_from );
         TEST_ASSERT( w.public_base().s == base::copy_constructed );
         TEST_ASSERT( v.public_base().number == 1 );
         TEST_ASSERT( w.public_base().number == 1 );
      }
      {
         counter = 0;
         based v = 42;
         based w = true;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( w == true );
         TEST_ASSERT( v.public_base().s == base::move_constructed );  // From defaulted second argument to basic_value::basic_value().
         TEST_ASSERT( w.public_base().s == base::move_constructed );  // From defaulted second argument to basic_value::basic_value().
         TEST_ASSERT( v.public_base().number == 1 );
         TEST_ASSERT( w.public_base().number == 2 );
         v = w;
         TEST_ASSERT( v == true );
         TEST_ASSERT( w == true );
         TEST_ASSERT( v.public_base().s == base::move_assigned );          // From by-value parameter to basic_value::operator=.
         TEST_ASSERT( w.public_base().s == base::copy_constructed_from );  // To by-value parameter to basic_value::operator=.
         TEST_ASSERT( v.public_base().number == 2 );
         TEST_ASSERT( w.public_base().number == 2 );
      }
      {
         counter = 0;
         based v = 42;
         based w = true;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( w == true );
         TEST_ASSERT( v.public_base().s == base::move_constructed );  // From defaulted second argument to basic_value::basic_value().
         TEST_ASSERT( w.public_base().s == base::move_constructed );  // From defaulted second argument to basic_value::basic_value().
         TEST_ASSERT( v.public_base().number == 1 );
         TEST_ASSERT( w.public_base().number == 2 );
         v = std::move( w );
         TEST_ASSERT( v == true );
         TEST_ASSERT( v.public_base().s == base::move_assigned );
         TEST_ASSERT( w.public_base().s == base::move_constructed_from );  // To by-value parameter to basic_value::operator=.
         TEST_ASSERT( v.public_base().number == 2 );
         TEST_ASSERT( w.public_base().number == 0 );
      }
      {
         counter = 0;
         based v = 42;
         based w = true;
         TEST_ASSERT( v == 42 );
         TEST_ASSERT( w == true );
         TEST_ASSERT( v.public_base().number == 1 );
         TEST_ASSERT( w.public_base().number == 2 );
         std::swap( v, w );
         TEST_ASSERT( v == true );
         TEST_ASSERT( w == 42 );
         TEST_ASSERT( v.public_base().s == base::move_assigned );
         TEST_ASSERT( w.public_base().s == base::move_assigned );
         TEST_ASSERT( v.public_base().number == 2 );
         TEST_ASSERT( w.public_base().number == 1 );
      }
   }

}  // namespace tao::json

#include "main.hpp"
