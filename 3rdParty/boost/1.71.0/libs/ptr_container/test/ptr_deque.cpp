//
// Boost.Pointer Container
//
//  Copyright Thorsten Ottosen 2003-2005. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/ptr_container/
//

#include <boost/test/unit_test.hpp>
#include "sequence_test_data.hpp"
#include <boost/ptr_container/ptr_deque.hpp>
#include <boost/ptr_container/detail/ptr_container_disable_deprecated.hpp>

#if defined(BOOST_PTR_CONTAINER_DISABLE_DEPRECATED)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

void test_ptr_deque()
{
    reversible_container_test< ptr_deque<Base>, Base, Derived_class >();
    reversible_container_test< ptr_deque<Value>, Value, Value >();
    reversible_container_test< ptr_deque< nullable<Base> >, Base, Derived_class >();
    reversible_container_test< ptr_deque< nullable<Value> >, Value, Value >();

    container_assignment_test< ptr_deque<Base>, ptr_deque<Derived_class>, 
                               Derived_class>();
    container_assignment_test< ptr_deque< nullable<Base> >, 
                               ptr_deque< nullable<Derived_class> >, 
                               Derived_class>();
    container_assignment_test< ptr_deque< nullable<Base> >, 
                               ptr_deque<Derived_class>, 
                               Derived_class>();
    container_assignment_test< ptr_deque<Base>, 
                               ptr_deque< nullable<Derived_class> >, 
                               Derived_class>();                           

    test_transfer< ptr_deque<Derived_class>, ptr_deque<Base>, Derived_class>();
    
    random_access_algorithms_test< ptr_deque<int> >();
    ptr_deque<int> di;
    di.push_front( new int(0) );
    std::size_t size = 1u;
    BOOST_CHECK_EQUAL( di.size(), size );
#ifndef BOOST_NO_AUTO_PTR
    di.push_front( std::auto_ptr<int>( new int(1) ) );
    ++size;
#endif
#ifndef BOOST_NO_CXX11_SMART_PTR
    di.push_front( std::unique_ptr<int>( new int(2) ) );
    ++size;
#endif
    BOOST_CHECK_EQUAL( di.size(), size );
}

#if defined(BOOST_PTR_CONTAINER_DISABLE_DEPRECATED)
#pragma GCC diagnostic pop
#endif

using boost::unit_test::test_suite;

test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite* test = BOOST_TEST_SUITE( "Pointer Container Test Suite" );

    test->add( BOOST_TEST_CASE( &test_ptr_deque ) );

    return test;
}

