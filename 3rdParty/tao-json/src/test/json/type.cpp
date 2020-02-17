// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/type.hpp>

namespace tao::json
{
   void unit_test()
   {
      TEST_ASSERT( to_string( type::UNINITIALIZED ) == std::string( "uninitialized" ) );

      TEST_ASSERT( to_string( type::NULL_ ) == std::string( "null" ) );
      TEST_ASSERT( to_string( type::BOOLEAN ) == std::string( "boolean" ) );
      TEST_ASSERT( to_string( type::SIGNED ) == std::string( "signed" ) );
      TEST_ASSERT( to_string( type::UNSIGNED ) == std::string( "unsigned" ) );
      TEST_ASSERT( to_string( type::DOUBLE ) == std::string( "double" ) );

      TEST_ASSERT( to_string( type::STRING ) == std::string( "string" ) );
      TEST_ASSERT( to_string( type::STRING_VIEW ) == std::string( "string_view" ) );
      TEST_ASSERT( to_string( type::BINARY ) == std::string( "binary" ) );
      TEST_ASSERT( to_string( type::BINARY_VIEW ) == std::string( "binary_view" ) );
      TEST_ASSERT( to_string( type::ARRAY ) == std::string( "array" ) );
      TEST_ASSERT( to_string( type::OBJECT ) == std::string( "object" ) );

      TEST_ASSERT( to_string( type::VALUE_PTR ) == std::string( "value_ptr" ) );
      TEST_ASSERT( to_string( type::OPAQUE_PTR ) == std::string( "opaque_ptr" ) );

      TEST_ASSERT( to_string( type( 42 ) ) == std::string( "unknown" ) );
   }

}  // namespace tao::json

#include "main.hpp"
