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
 
#include <boost/ptr_container/indirect_fun.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/assign/list_inserter.hpp> 
#include <boost/test/test_tools.hpp>
#include <algorithm>
#include <functional>
#include <string>

bool lesser_than( const std::string& l, const std::string& r )
{
    return l < r;
}


void test_fun()
{
    using namespace boost;
    ptr_vector<std::string> vec;

    indirect_fun< std::less<std::string> > fun;

    std::string s1("bar");
    std::string* ptr1 = &s1;
    std::string s2("foo");
    std::string* ptr2 = &s2;
    BOOST_CHECK( fun( ptr1, ptr2 ) == true );
    
    void*       vptr1  = ptr1;
    void*       vptr2  = ptr2;
    
    void_ptr_indirect_fun< std::less<std::string>, std::string> cast_fun;
    BOOST_CHECK( cast_fun( vptr1, vptr2 ) == true );
    
    assign::push_back( vec )( new std::string("aa") )
                            ( new std::string("bb") )
                            ( new std::string("dd") )
                            ( new std::string("cc") )
                            ( new std::string("a") );
                              
    std::sort( vec.begin().base(), vec.end().base(), cast_fun );
    BOOST_CHECK( vec[0] == "a" );
    BOOST_CHECK( vec[4] == "dd" );

    std::sort( vec.begin().base(), vec.end().base(), 
               make_void_ptr_indirect_fun<std::string>( &lesser_than ) );
    BOOST_CHECK( vec[1] == "aa" );
    BOOST_CHECK( vec[2] == "bb" );    

    int i1 = 2;
    void *iptr1 = &i1;
    int i2 = 3;
    void* iptr2 = &i2;

    void_ptr_indirect_fun<std::less<int>, int> int_cast_fun;
    BOOST_CHECK( int_cast_fun(iptr1,iptr2) );
    
}

#include <boost/test/unit_test.hpp>
using boost::unit_test::test_suite;

test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite* test = BOOST_TEST_SUITE( "Pointer Container Test Suite" );

    test->add( BOOST_TEST_CASE( &test_fun ) );

    return test;
}


