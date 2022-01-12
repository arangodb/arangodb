// Copyright 2019 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#include <boost/throw_exception.hpp>
#include <stdexcept>

int main()
{
    try
    {
        boost::throw_exception( std::runtime_error( "" ) );
        return 1;
    }
    catch( std::runtime_error const& )
    {
        return 0;
    }
}
