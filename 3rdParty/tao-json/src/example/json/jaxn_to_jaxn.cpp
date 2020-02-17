// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <iostream>

#include <tao/json.hpp>

int main( int argc, char** argv )
{
   if( argc != 2 ) {
      std::cerr << "usage: " << argv[ 0 ] << " file.jaxn" << std::endl;
      std::cerr << "  parses the jaxn file and writes it to stdout as jaxn" << std::endl;
      return 1;
   }
   tao::json::jaxn::events::to_stream consumer( std::cout );
   tao::json::jaxn::events::from_file( consumer, argv[ 1 ] );
   return 0;
}
