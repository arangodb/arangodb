//Copyright (c) 2006-2009 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_EXCEPTION_DISABLE

#include <boost/config.hpp>

#if !defined( BOOST_NO_EXCEPTIONS )
#   error This program requires exception handling disabled.
#endif

#include <boost/throw_exception.hpp>
#include <stdlib.h>

class my_exception: public std::exception { };

namespace
boost
    {
    void
    throw_exception( std::exception const & )
        {
        exit(0);
        }
    }

int
main()
    {
    boost::throw_exception(my_exception());
    return 1;
    }
