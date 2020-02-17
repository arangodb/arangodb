// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include <tao/json/value.hpp>

#include <tao/json/events/discard.hpp>
#include <tao/json/events/from_value.hpp>

namespace tao::json
{
   template< typename Consumer, std::uint64_t Min, std::uint64_t Max >
   struct validate_integer
      : public Consumer
   {
      static_assert( Max >= Min );
      static_assert( Max <= std::uint64_t( ( std::numeric_limits< std::int64_t >::max )() ), "Max may not be larger than 2^63-1" );

      using Consumer::Consumer;

      void number( const std::int64_t v )
      {
         if( ( v < std::int64_t( Min ) ) || ( v > std::int64_t( Max ) ) ) {
            throw std::runtime_error( "integer range violated: " + std::to_string( v ) );
         }
         Consumer::number( v );
      }

      void number( const std::uint64_t v )
      {
         if( ( v < Min ) || ( v > Max ) ) {
            throw std::runtime_error( "unsigned range violated: " + std::to_string( v ) );
         }
         Consumer::number( v );
      }

      void number( const double v ) noexcept( noexcept( std::declval< Consumer >().number( v ) ) )
      {
         Consumer::number( v );
      }
   };

}  // namespace tao::json

int main( int /*unused*/, char** /*unused*/ )
{
   tao::json::value v = { { "a", 20 }, { "b", 30 } };
   tao::json::validate_integer< tao::json::events::discard, 10, 40 > consumer;
   tao::json::events::from_value( consumer, v );
   return 0;
}
