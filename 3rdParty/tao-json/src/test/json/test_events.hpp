// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_SRC_TEST_JSON_TEST_EVENTS_HPP
#define TAO_JSON_SRC_TEST_JSON_TEST_EVENTS_HPP

#include <tao/json.hpp>

#include "test.hpp"

namespace tao::json::test
{
   enum event_type
   {
      NULL_,
      BOOLEAN,
      SIGNED,
      UNSIGNED,
      DOUBLE,
      STRING,  // TODO: Differentiate string from string_view...
      BINARY,  // TODO: Differentiate binary from binary_view...
      KEY,     // TODO: Differentiate string from string_view...
      BEGIN_ARRAY,
      ELEMENT,
      END_ARRAY,
      BEGIN_OBJECT,
      MEMBER,
      END_OBJECT
   };

   struct event_data
   {
      event_data() = default;

      event_data( const event_type in_t )
         : t( in_t )
      {}

      event_data( const bool in_b )
         : t( event_type::BOOLEAN ),
           b( in_b )
      {}

      event_type t{ event_type::NULL_ };

      bool b = false;
      std::string s;
      std::vector< std::byte > x;
      std::int64_t i = 0;
      std::uint64_t u = 0;
      double d = 0.0;
   };

   [[nodiscard]] inline event_data int64( const std::int64_t i )
   {
      event_data ev( event_type::SIGNED );
      ev.i = i;
      return ev;
   }

   [[nodiscard]] inline event_data uint64( const std::uint64_t u )
   {
      event_data ev( event_type::UNSIGNED );
      ev.u = u;
      return ev;
   }

   [[nodiscard]] inline event_data string( const std::string& s )
   {
      event_data ev( event_type::STRING );
      ev.s = s;
      return ev;
   }

   [[nodiscard]] inline event_data key( const std::string& s )
   {
      event_data ev( event_type::KEY );
      ev.s = s;
      return ev;
   }

   class consumer
   {
   public:
      consumer( std::initializer_list< event_data > events )
         : m_e( events )
      {}

      consumer( consumer&& ) = delete;
      consumer( const consumer& ) = delete;

      void operator=( consumer&& ) = delete;
      void operator=( const consumer& ) = delete;

      ~consumer()
      {
         TEST_ASSERT( m_o.is_complete() );
         TEST_ASSERT( m_i == m_e.size() );
      }

      void null()
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::NULL_ );
         increment();
         m_o.null();
      }

      void boolean( const bool b )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::BOOLEAN );
         TEST_ASSERT( m_e[ m_i ].b == b );
         increment();
         m_o.boolean( b );
      }

      void number( const std::int64_t i )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::SIGNED );
         TEST_ASSERT( m_e[ m_i ].i == i );
         increment();
         m_o.number( i );
      }

      void number( const std::uint64_t u )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::UNSIGNED );
         TEST_ASSERT( m_e[ m_i ].u == u );
         increment();
         m_o.number( u );
      }

      void number( const double d )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::DOUBLE );
         TEST_ASSERT( m_e[ m_i ].d == d );
         increment();
         m_o.number( d );
      }

      void string( const std::string_view s )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::STRING );
         TEST_ASSERT( m_e[ m_i ].s == s );
         increment();
         m_o.string( s );
      }

      void binary( const std::vector< std::byte >& x )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::BINARY );
         TEST_ASSERT( m_e[ m_i ].x == x );
         increment();
         m_o.binary( x );
      }

      void key( const std::string_view s )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::KEY );
         TEST_ASSERT( m_e[ m_i ].s == s );
         increment();
         m_o.key( s );
      }

      void begin_array()
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::BEGIN_ARRAY );
         increment();
         m_o.begin_array();
      }

      void begin_array( const std::size_t n )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::BEGIN_ARRAY );
         increment();
         m_o.begin_array( n );
      }

      void element()
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::ELEMENT );
         increment();
         m_o.element();
      }

      void end_array()
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::END_ARRAY );
         increment();
         m_o.end_array();
      }

      void end_array( const std::size_t n )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::END_ARRAY );
         increment();
         m_o.end_array( n );
      }

      void begin_object()
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::BEGIN_OBJECT );
         increment();
         m_o.begin_object();
      }

      void begin_object( const std::size_t n )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::BEGIN_OBJECT );
         increment();
         m_o.begin_object( n );
      }

      void member()
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::MEMBER );
         increment();
         m_o.member();
      }

      void end_object()
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::END_OBJECT );
         increment();
         m_o.end_object();
      }

      void end_object( const std::size_t n )
      {
         TEST_ASSERT( m_e[ m_i ].t == event_type::END_OBJECT );
         increment();
         m_o.end_object( n );
      }

   private:
      std::size_t m_i = 0;
      events::validate_event_order m_o;
      const std::vector< event_data > m_e;

      void increment()
      {
         TEST_ASSERT( m_i < m_e.size() );
         if( m_i < m_e.size() ) {
            ++m_i;
         }
      }
   };

}  // namespace tao::json::test

#endif
