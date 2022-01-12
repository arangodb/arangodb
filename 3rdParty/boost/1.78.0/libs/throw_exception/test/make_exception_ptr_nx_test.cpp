// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
# pragma warning(disable: 4577) // noexcept used without /EHsc
# pragma warning(disable: 4530) // C++ exception handler used
#endif

#include <boost/exception_ptr.hpp>

class my_exception: public std::exception {};

int main()
{
    boost::make_exception_ptr( my_exception() );
}

namespace boost
{

// shared_ptr needs this
void throw_exception( std::exception const & )
{
    std::exit( 1 );
}

} // namespace boost
