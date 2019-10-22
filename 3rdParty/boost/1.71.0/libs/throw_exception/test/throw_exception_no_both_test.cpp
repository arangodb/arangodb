//Copyright (c) 2006-2009 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_NO_EXCEPTIONS
#define BOOST_EXCEPTION_DISABLE

#include <boost/throw_exception.hpp>
#include <cstdlib>

class my_exception: public std::exception {};

int main()
{
    boost::throw_exception( my_exception() );
    return 1;
}

namespace boost
{

void throw_exception( std::exception const & )
{
    std::exit( 0 );
}

} // namespace boost
