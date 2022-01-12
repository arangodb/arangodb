/*=============================================================================
    Copyright (c) 2001-2010 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/include/qi_eps.hpp>

#include <boost/spirit/include/qi_lazy.hpp>
#include <boost/spirit/include/qi_not_predicate.hpp>
#include <boost/phoenix/core.hpp>
#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using boost::spirit::eps;

    {
        BOOST_TEST((test("", eps)));
        BOOST_TEST((test("xxx", eps, false)));
        BOOST_TEST((!test("", !eps))); // not predicate
    }

    {   // test non-lazy semantic predicate

        BOOST_TEST((test("", eps(true))));
        BOOST_TEST((!test("", eps(false))));
        BOOST_TEST((test("", !eps(false)))); // not predicate
    }

    {   // test lazy semantic predicate

        using boost::phoenix::val;

        BOOST_TEST((test("", eps(val(true)))));
        BOOST_TEST((!test("", eps(val(false)))));
        BOOST_TEST((test("", !eps(val(false))))); // not predicate
    }

    return boost::report_errors();
}
