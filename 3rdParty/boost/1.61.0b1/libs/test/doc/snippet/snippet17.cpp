//  (C) Copyright Gennadiy Rozental 2001-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org/libs/test for the library home page.
//

//[snippet17
#define __BOOST_TEST_MODULE__ const_string test
#include <boost/test/unit_test.hpp>

__BOOST_AUTO_TEST_CASE__( constructors_test )
{
  //...
}

__BOOST_AUTO_TEST_CASE__( data_access_test )
{
  const_string cs1( "test_string" );                                // 1 //
  __BOOST_TEST__( cs1[(size_t)0] == 't' );
  __BOOST_TEST__( cs1[(size_t)4] == '_' );
  __BOOST_TEST__( cs1[cs1.length()-1] == 'g' );

  __BOOST_TEST__( cs1[(size_t)0] == cs1.at( 0 ) );                      // 2 //
  __BOOST_TEST__( cs1[(size_t)2] == cs1.at( 5 ) );
  __BOOST_TEST__( cs1.at( cs1.length() - 1 ) == 'g' );

  BOOST_CHECK_THROW( cs1.at( cs1.length() ), std::out_of_range );   // 3 //
}
//]
