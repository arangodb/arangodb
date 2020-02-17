// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/to_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   void unit_test()
   {
      value v{ { "foo", 1 } };
      const value cv{ { "fuu", 2 } };

      const value v2{ { "bar", &v } };    // Store a non-owning const pointer to v.
      const value cv2{ { "baz", &cv } };  // Store a non-owning const pointer to cv.

      TEST_ASSERT( to_string( v ) == "{\"foo\":1}" );
      TEST_ASSERT( to_string( cv ) == "{\"fuu\":2}" );

      TEST_ASSERT( to_string( v2 ) == "{\"bar\":{\"foo\":1}}" );

      TEST_ASSERT( v2.at( "bar" ).is_value_ptr() );

      TEST_ASSERT( to_string( cv2 ) == "{\"baz\":{\"fuu\":2}}" );

      TEST_ASSERT( cv2.at( "baz" ).is_value_ptr() );
   }

}  // namespace tao::json

#include "main.hpp"
