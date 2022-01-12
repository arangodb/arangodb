//  Unit test for boost::any.
//
//  See http://www.boost.org for most recent version, including documentation.
//
//  Copyright Antony Polukhin, 2013-2019.
//  Copyright Ruslan Arutyunyan, 2019-2021.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).

#include <cstdlib>
#include <string>
#include <utility>

#include <boost/any/basic_any.hpp>
#include "test.hpp"
#include <boost/move/move.hpp>

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES

int main()
{
    BOOST_STATIC_ASSERT(false);
    return EXIT_SUCCESS;
}

#else


int main()
{
    boost::anys::basic_any<> const cvalue(10);
    int i = boost::any_cast<int&&>(cvalue);
    (void)i;
    return EXIT_SUCCESS;
}

#endif

