//
// Test for no_exceptions_support.hpp
//
// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/no_exceptions_support.hpp>
#include <boost/core/quick_exit.hpp>
#include <boost/throw_exception.hpp>
#include <exception>

void f()
{
    boost::throw_exception( std::exception() );
}

int main()
{
    BOOST_TRY
    {
        f();
    }
    BOOST_CATCH( std::exception const& )
    {
        return 0;
    }
    BOOST_CATCH( ... )
    {
        return 1;
    }
    BOOST_CATCH_END

    return 1;
}

#if defined(BOOST_NO_EXCEPTIONS)

namespace boost
{

void throw_exception( std::exception const& )
{
    boost::quick_exit( 0 );
}

}

#endif
