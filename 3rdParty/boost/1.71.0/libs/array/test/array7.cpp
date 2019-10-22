/* tests using std::get on boost:array
 * (C) Copyright Marshall Clow 2012
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <string>
#include <iostream>
#include <boost/array.hpp>
#include <algorithm>
#ifndef BOOST_NO_CXX11_HDR_ARRAY
#include <array>
#endif

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

namespace {

    #ifndef BOOST_NO_CXX11_HDR_ARRAY
    template< class T >
    void    RunStdTests()
    {
        typedef boost::array< T, 5 >    test_type;
        typedef T arr[5];
        test_type           test_case; //   =   { 1, 1, 2, 3, 5 };
    
        T &aRef = std::get<0> ( test_case );
        BOOST_CHECK ( &*test_case.begin () == &aRef );
        
        const T &caRef = std::get<0> ( test_case );
        BOOST_CHECK ( &*test_case.cbegin () == &caRef );
    }
    #endif

    template< class T >
    void    RunBoostTests()
    {
        typedef boost::array< T, 5 >    test_type;
        typedef T arr[5];
        test_type           test_case; //   =   { 1, 1, 2, 3, 5 };
    
        T &aRef = boost::get<0> ( test_case );
        BOOST_CHECK ( &*test_case.begin () == &aRef );
        
        const T &caRef = boost::get<0> ( test_case );
        BOOST_CHECK ( &*test_case.cbegin () == &caRef );
    }

}

BOOST_AUTO_TEST_CASE( test_main )
{
    RunBoostTests< bool >();
    RunBoostTests< void * >();
    RunBoostTests< long double >();
    RunBoostTests< std::string >();

#ifndef BOOST_NO_CXX11_HDR_ARRAY
    RunStdTests< bool >();
    RunStdTests< void * >();
    RunStdTests< long double >();
    RunStdTests< std::string >();
#endif
}

