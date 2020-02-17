// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/events/produce.hpp>
#include <tao/json/from_string.hpp>
#include <tao/json/produce.hpp>
#include <tao/json/self_contained.hpp>
#include <tao/json/to_stream.hpp>
#include <tao/json/to_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   namespace events
   {
      template< template< typename... > class Traits, typename Consumer >
      struct array_t
      {
         Consumer* c_;

         explicit array_t( Consumer& c ) noexcept( noexcept( c_->begin_array() ) )
            : c_( &c )
         {
            c_->begin_array();
         }

         array_t( const array_t& ) = delete;

         array_t( array_t&& r ) noexcept
            : c_( r.c_ )
         {
            r.c_ = nullptr;
         }

         ~array_t()
         {
            if( c_ ) {
               c_->end_array();
            }
         }

         void operator=( const array_t& ) = delete;
         void operator=( array_t&& ) = delete;

         template< typename T >
         array_t& push_back( T&& v )
         {
            assert( c_ );
            produce< Traits >( *c_, std::forward< T >( v ) );
            c_->element();
            return *this;
         }
      };

      template< template< typename... > class Traits, typename Consumer >
      struct object_t
      {
         Consumer* c_;

         explicit object_t( Consumer& c ) noexcept( noexcept( c_->begin_object() ) )
            : c_( &c )
         {
            c_->begin_object();
         }

         object_t( const object_t& ) = delete;

         object_t( object_t&& r ) noexcept
            : c_( r.c_ )
         {
            r.c_ = nullptr;
         }

         ~object_t()
         {
            if( c_ ) {
               c_->end_object();
            }
         }

         void operator=( const object_t& ) = delete;
         void operator=( object_t&& ) = delete;

         template< typename T >
         object_t& insert( const std::string& k, T&& v )
         {
            assert( c_ );
            c_->key( k );
            produce< Traits >( *c_, std::forward< T >( v ) );
            c_->member();
            return *this;
         }
      };

      template< template< typename... > class Traits, typename Consumer >
      [[nodiscard]] array_t< Traits, Consumer > array( Consumer& c ) noexcept( noexcept( array_t< Traits, Consumer >( c ) ) )
      {
         return array_t< Traits, Consumer >( c );
      }

      template< template< typename... > class Traits, typename Consumer >
      [[nodiscard]] object_t< Traits, Consumer > object( Consumer& c ) noexcept( noexcept( object_t< Traits, Consumer >( c ) ) )
      {
         return object_t< Traits, Consumer >( c );
      }

   }  // namespace events

   struct point
   {
      double x = 1.0;
      double y = 2.0;
   };

   template<>
   struct traits< point >
   {
      template< template< typename... > class Traits, typename Consumer >
      static void produce( Consumer& consumer, const point& data )
      {
         auto a = events::array< Traits >( consumer );
         a.push_back( data.x );
         a.push_back( data.y );
      }
   };

   struct employee
   {
      std::string name = "Isidor";
      std::string position = "CEO";
      std::uint64_t income = 42;
      point coordinates;
   };

   template<>
   struct traits< employee >
   {
      template< template< typename... > class Traits, typename Consumer >
      static void produce( Consumer& consumer, const employee& data )
      {
         auto o = events::object< Traits >( consumer );
         o.insert( "name", data.name );
         if( !data.position.empty() ) {
            o.insert( "position", data.position );
         }
         o.insert( "income", data.income );
         o.insert( "coordinates", data.coordinates );
      }
   };

   template<>
   struct traits< const employee* >
   {
      static void assign( value& v, const employee* e )
      {
         v.set_opaque_ptr( e );
      }
   };

   void unit_test()
   {
      const employee e{};

      value v1 = {
         { "account", 1 },
         { "employee", &e }
      };

      const auto s1 = to_string( from_string( to_string( v1 ) ) );

      value v2 = v1;

      make_self_contained( v2 );

      const auto s2 = to_string( from_string( to_string( v2 ) ) );

      TEST_ASSERT( s1 == s2 );

      value v3 = v2;

      TEST_ASSERT( v2 == v3 );

      const auto s3 = produce::to_string( e );

      TEST_ASSERT( s3 == "{\"name\":\"Isidor\",\"position\":\"CEO\",\"income\":42,\"coordinates\":[1.0,2.0]}" );
   }

}  // namespace tao::json

#include "main.hpp"
