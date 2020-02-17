// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#ifdef _MSC_VER
#pragma warning( disable : 4702 )
#endif

#include <tao/json/contrib/schema.hpp>

#include <tao/json/events/compare.hpp>
#include <tao/json/events/debug.hpp>
#include <tao/json/events/discard.hpp>
#include <tao/json/events/hash.hpp>
#include <tao/json/events/ref.hpp>
#include <tao/json/events/statistics.hpp>
#include <tao/json/events/tee.hpp>
#include <tao/json/events/to_pretty_stream.hpp>
#include <tao/json/events/to_stream.hpp>
#include <tao/json/events/to_string.hpp>
#include <tao/json/events/to_value.hpp>
#include <tao/json/events/transformer.hpp>
#include <tao/json/events/validate_event_order.hpp>
#include <tao/json/events/validate_keys.hpp>
#include <tao/json/events/virtual_ref.hpp>

#include <tao/json/events/binary_to_base64.hpp>
#include <tao/json/events/binary_to_base64url.hpp>
#include <tao/json/events/binary_to_exception.hpp>
#include <tao/json/events/binary_to_hex.hpp>
#include <tao/json/events/key_camel_case_to_snake_case.hpp>
#include <tao/json/events/key_snake_case_to_camel_case.hpp>
#include <tao/json/events/limit_nesting_depth.hpp>
#include <tao/json/events/limit_value_count.hpp>
#include <tao/json/events/non_finite_to_exception.hpp>
#include <tao/json/events/non_finite_to_null.hpp>
#include <tao/json/events/non_finite_to_string.hpp>
#include <tao/json/events/prefer_signed.hpp>
#include <tao/json/events/prefer_unsigned.hpp>

#include <tao/json/cbor/events/to_stream.hpp>
#include <tao/json/cbor/events/to_string.hpp>

#include <tao/json/jaxn/events/to_pretty_stream.hpp>
#include <tao/json/jaxn/events/to_stream.hpp>

#include <tao/json/msgpack/events/to_stream.hpp>
#include <tao/json/msgpack/events/to_string.hpp>

#include <tao/json/ubjson/events/to_stream.hpp>
#include <tao/json/ubjson/events/to_string.hpp>

namespace tao::json
{
   template< typename Consumer >
   void check_consumer_impl( Consumer* c = nullptr )
   {
      if( c == nullptr ) {
         return;
      }

      std::string s;
      std::string k;
      std::string_view sv;
      std::vector< std::byte > x;
      tao::binary_view xv;

      // each consumer *must* accept the following events (type-wise, not actual order/values)

      c->null();

      c->boolean( true );

      c->number( int64_t( 0 ) );
      c->number( uint64_t( 0 ) );
      c->number( double( 0 ) );

      const char* p = "";
      c->string( "" );
      c->string( p );
      c->string( s );
      c->string( std::move( s ) );
      c->string( sv );

      c->binary( x );
      c->binary( std::move( x ) );
      c->binary( xv );

      c->begin_array();
      c->begin_array( std::size_t( 0 ) );

      c->element();

      c->end_array();
      c->end_array( std::size_t( 0 ) );

      c->begin_object();
      c->begin_object( std::size_t( 0 ) );

      c->key( "" );
      c->key( k );
      c->key( std::move( k ) );
      c->key( sv );

      c->member();

      c->end_object();
      c->end_object( std::size_t( 0 ) );
   }

   template< typename Consumer >
   void check_consumer()
   {
      check_consumer_impl< Consumer >();

      check_consumer_impl< events::ref< Consumer > >();
      check_consumer_impl< events::virtual_ref< Consumer > >();

      check_consumer_impl< events::tee< Consumer > >();
      check_consumer_impl< events::tee< events::discard, Consumer > >();
      check_consumer_impl< events::tee< Consumer, events::discard > >();
      check_consumer_impl< events::tee< Consumer, Consumer > >();
      check_consumer_impl< events::tee< Consumer, events::discard, Consumer > >();

      check_consumer_impl< events::validate_keys< Consumer, pegtl::success > >();

      check_consumer_impl< events::limit_value_count< Consumer, 2 > >();
      check_consumer_impl< events::limit_nesting_depth< Consumer, 2 > >();

      check_consumer_impl< events::transformer< Consumer > >();
      check_consumer_impl< events::transformer< Consumer, events::binary_to_base64 > >();
      check_consumer_impl< events::transformer< Consumer, events::binary_to_base64url > >();
      check_consumer_impl< events::transformer< Consumer, events::binary_to_exception > >();
      check_consumer_impl< events::transformer< Consumer, events::binary_to_hex > >();
      check_consumer_impl< events::transformer< Consumer, events::key_camel_case_to_snake_case > >();
      check_consumer_impl< events::transformer< Consumer, events::key_snake_case_to_camel_case > >();
      check_consumer_impl< events::transformer< Consumer, events::non_finite_to_exception > >();
      check_consumer_impl< events::transformer< Consumer, events::non_finite_to_null > >();
      check_consumer_impl< events::transformer< Consumer, events::non_finite_to_string > >();
      check_consumer_impl< events::transformer< Consumer, events::prefer_signed > >();
      check_consumer_impl< events::transformer< Consumer, events::prefer_unsigned > >();
   }

   void unit_test()
   {
      check_consumer< events::compare >();
      check_consumer< events::debug >();
      check_consumer< events::discard >();
      check_consumer< events::hash >();
      check_consumer< events::statistics >();
      check_consumer< events::to_pretty_stream >();
      check_consumer< events::to_stream >();
      check_consumer< events::to_string >();
      check_consumer< events::to_value >();
      check_consumer< events::validate_event_order >();

      check_consumer< events::tee<> >();

      check_consumer< cbor::events::to_stream >();
      check_consumer< cbor::events::to_string >();

      check_consumer< jaxn::events::to_pretty_stream >();
      check_consumer< jaxn::events::to_stream >();

      check_consumer< msgpack::events::to_stream >();
      check_consumer< msgpack::events::to_string >();

      check_consumer< ubjson::events::to_stream >();
      check_consumer< ubjson::events::to_string >();

      check_consumer< internal::schema_consumer< traits > >();
   }

}  // namespace tao::json

#include "main.hpp"
