// Copyright 2019 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
# pragma warning(disable: 4577) // noexcept used without /EHsc
# pragma warning(disable: 4530) // C++ exception handler used
#endif

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
