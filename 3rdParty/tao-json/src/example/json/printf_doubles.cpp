// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <cstdio>
#include <iostream>

#include <tao/json.hpp>

namespace tao::json::events
{
   struct printf_doubles
      : public to_stream
   {
      using to_stream::to_stream;

      void number( const double v )
      {
         next();
         if( !std::isfinite( v ) ) {
            // if this throws, consider using non_finite_to_* transformers
            throw std::runtime_error( "non-finite double value invalid for JSON string representation" );
         }
         char buffer[ 32 ];
         const std::size_t n = std::snprintf( buffer, sizeof( buffer ), "%0.2f", v );
         os.write( buffer, n );
      }

      void number( const std::int64_t v )
      {
         to_stream::number( v );
      }

      void number( const std::uint64_t v )
      {
         to_stream::number( v );
      }
   };

}  // namespace tao::json::events

int main( int argc, char** argv )
{
   if( argc != 2 ) {
      std::cerr << "usage: " << argv[ 0 ] << " file.json" << std::endl;
      std::cerr << "  parses the json file and writes it to stdout with '%0.2f' for doubles" << std::endl;
      return 1;
   }
   tao::json::events::printf_doubles consumer( std::cout );
   tao::json::events::from_file( consumer, argv[ 1 ] );
   return 0;
}
