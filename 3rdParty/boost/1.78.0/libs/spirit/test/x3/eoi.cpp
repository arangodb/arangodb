/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/home/x3.hpp>

#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using boost::spirit::x3::eoi;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(eoi);

    {
        BOOST_TEST((test("", eoi)));
        BOOST_TEST(!(test("x", eoi)));
    }

    {
        BOOST_TEST(what(eoi) == "eoi");
    }

    return boost::report_errors();
}
