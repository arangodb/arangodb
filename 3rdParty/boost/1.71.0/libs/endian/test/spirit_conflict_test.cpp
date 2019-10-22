// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning( disable: 4510 ) // default constructor not generated
# pragma warning( disable: 4512 ) // assignment operator not generated
# pragma warning( disable: 4610 ) // class can never be instantiated
#endif

#include <boost/spirit/include/qi.hpp>
#include <boost/endian/arithmetic.hpp>

struct record
{
    boost::endian::big_int16_t type;

    record( boost::int16_t t )
    {
        type = t;
    }
};
