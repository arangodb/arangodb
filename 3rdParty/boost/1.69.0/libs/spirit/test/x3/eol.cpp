/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>

#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using boost::spirit::x3::eol;

    {
        BOOST_TEST((test("\r\n", eol)));
        BOOST_TEST((test("\r", eol)));
        BOOST_TEST((test("\n", eol)));
        BOOST_TEST((!test("\n\r", eol)));
        BOOST_TEST((!test("", eol)));
    }

    {
        BOOST_TEST(what(eol) == "eol");
    }

    return boost::report_errors();
}
