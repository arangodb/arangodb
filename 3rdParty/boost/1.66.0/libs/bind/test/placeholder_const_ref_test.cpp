//
//  placeholder_const_ref_test.cpp - forming a const& to _1
//
//  Copyright 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/bind/placeholders.hpp>
#include <boost/is_placeholder.hpp>
#include <boost/core/lightweight_test.hpp>

//

template<class T> void test( T const &, int i )
{
    BOOST_TEST_EQ( boost::is_placeholder<T>::value, i );
}

int main()
{
    using namespace boost::placeholders;

    test( _1, 1 );
    test( _2, 2 );
    test( _3, 3 );
    test( _4, 4 );
    test( _5, 5 );
    test( _6, 6 );
    test( _7, 7 );
    test( _8, 8 );
    test( _9, 9 );

    return boost::report_errors();
}
