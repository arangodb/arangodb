// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/from_file.hpp>
#include <tao/json/from_string.hpp>
#include <tao/json/to_string.hpp>
#include <tao/json/value.hpp>

#include <fstream>
#include <string>

[[nodiscard]] std::string get_file_contents( const char* filename )
{
   std::ifstream in( filename, std::ios::in | std::ios::binary );
   if( !in.fail() ) {
      std::string contents;
      in.seekg( 0, std::ios::end );
      contents.resize( static_cast< std::string::size_type >( in.tellg() ) );
      in.seekg( 0, std::ios::beg );
      in.read( &contents[ 0 ], contents.size() );
      in.close();
      return contents;
   }
   throw std::runtime_error( "unable to read input file" );
}

namespace tao::json
{
   void unit_test()
   {
      const auto v = from_file( "tests/blns.json" );
      TEST_ASSERT( v.get_array().size() == 494 );
      const auto s = to_string( v, 2 ) + '\n';
      TEST_ASSERT( s == get_file_contents( "tests/blns.json" ) );
      const auto v2 = from_string( s );
      TEST_ASSERT( v2 == v );
   }

}  // namespace tao::json

#include "main.hpp"
