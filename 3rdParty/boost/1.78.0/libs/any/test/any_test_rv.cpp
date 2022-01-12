//  Unit test for boost::any.
//
//  See http://www.boost.org for most recent version, including documentation.
//
//  Copyright Antony Polukhin, 2013-2021.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).

#include <boost/any.hpp>

#include "move_test.hpp"

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES

int main()
{
    return EXIT_SUCCESS;
}

#else

int main() {
    return any_tests::move_tests<boost::any>::run_tests();
}

#endif

