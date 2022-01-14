
// Copyright 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>
#include <exception>
#include <cstdio>

namespace boost
{

void throw_exception( std::exception const & e )
{
    std::fprintf( stderr, "Exception: %s\n", e.what() );
    std::terminate();
}

} // namespace boost
