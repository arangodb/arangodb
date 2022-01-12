#include <boost/config.hpp>

#if defined(BOOST_MSVC)
#pragma warning(disable: 4786)  // identifier truncated in debug info
#pragma warning(disable: 4710)  // function not inlined
#pragma warning(disable: 4711)  // function selected for automatic inline expansion
#pragma warning(disable: 4514)  // unreferenced inline removed
#endif

//
//  bind_not_test.cpp - operator!
//
//  Copyright (c) 2005 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/bind/bind.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::placeholders;

//

template<class F, class A1, class R> void test( F f, A1 a1, R r )
{
    BOOST_TEST( f(a1) == r );
}

bool f( bool v )
{
    return v;
}

int g( int v )
{
    return v;
}

int main()
{
    test( !boost::bind( f, true ), 0, !f( true ) );
    test( !boost::bind( g, _1 ), 5, !g( 5 ) );
    test( boost::bind( f, !boost::bind( f, true ) ), 0, f( !f( true ) ) );
    test( boost::bind( f, !boost::bind( f, _1 ) ), true, f( !f( true ) ) );

    return boost::report_errors();
}
