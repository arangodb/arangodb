//
// Copyright 2013 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/utility/addressof.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

#if defined( BOOST_NO_CXX11_NULLPTR )

void nullptr_test()
{
}

#else

void nullptr_test()
{
    {
        auto x = nullptr;
        BOOST_TEST( boost::addressof(x) == &x );
    }

    {
        auto const x = nullptr;
        BOOST_TEST( boost::addressof(x) == &x );
    }

    {
        auto volatile x = nullptr;
        BOOST_TEST( boost::addressof(x) == &x );
    }

    {
        auto const volatile x = nullptr;
        BOOST_TEST( boost::addressof(x) == &x );
    }
}

#endif

int main()
{
    nullptr_test();
    return boost::report_errors();
}
