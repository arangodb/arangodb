// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"
#include "test_unhex.hpp"

#include <tao/json.hpp>

namespace tao::json
{
   void unit_test()
   {
      {
         cbor::parts_parser p( "", __FUNCTION__ );
         TEST_ASSERT( p.empty() );
      }
      {
         cbor::parts_parser p( test_unhex( "f6" ), __FUNCTION__ );
         TEST_ASSERT( p.null() == true );
         TEST_ASSERT( p.empty() );
      }
      {
         cbor::parts_parser p( test_unhex( "f4" ), __FUNCTION__ );
         TEST_ASSERT( p.boolean() == false );
         TEST_ASSERT( p.empty() );
      }
      {
         cbor::parts_parser p( test_unhex( "f5" ), __FUNCTION__ );
         TEST_ASSERT( p.boolean() == true );
         TEST_ASSERT( p.empty() );
      }
   }

}  // namespace tao::json

#include "main.hpp"
