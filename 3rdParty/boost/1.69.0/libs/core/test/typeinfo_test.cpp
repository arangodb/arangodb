//
// typeinfo_test.cpp
//
// Copyright (c) 2009 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/typeinfo.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>

int main()
{
    BOOST_TEST( BOOST_CORE_TYPEID( int ) == BOOST_CORE_TYPEID( int ) );
    BOOST_TEST( BOOST_CORE_TYPEID( int ) != BOOST_CORE_TYPEID( long ) );
    BOOST_TEST( BOOST_CORE_TYPEID( int ) != BOOST_CORE_TYPEID( void ) );

    boost::core::typeinfo const & ti = BOOST_CORE_TYPEID( int );

    boost::core::typeinfo const * pti = &BOOST_CORE_TYPEID( int );
    BOOST_TEST( *pti == ti );

    BOOST_TEST( ti == ti );
    BOOST_TEST( !( ti != ti ) );
    BOOST_TEST( !ti.before( ti ) );

    char const * nti = ti.name();
    std::cout << nti << std::endl;

    boost::core::typeinfo const & tv = BOOST_CORE_TYPEID( void );

    boost::core::typeinfo const * ptv = &BOOST_CORE_TYPEID( void );
    BOOST_TEST( *ptv == tv );

    BOOST_TEST( tv == tv );
    BOOST_TEST( !( tv != tv ) );
    BOOST_TEST( !tv.before( tv ) );

    char const * ntv = tv.name();
    std::cout << ntv << std::endl;

    BOOST_TEST( ti != tv );
    BOOST_TEST( !( ti == tv ) );

    BOOST_TEST( ti.before( tv ) != tv.before( ti ) );

    return boost::report_errors();
}
