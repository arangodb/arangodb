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
    using boost::spirit::x3::eps;
    using boost::spirit::x3::unused_type;

    {
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(eps);
        BOOST_TEST((test("", eps)));
        BOOST_TEST((test("xxx", eps, false)));
        //~ BOOST_TEST((!test("", !eps))); // not predicate $$$ Implement me! $$$
    }

    {   // test non-lazy semantic predicate

        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(eps(true));
        BOOST_TEST((test("", eps(true))));
        BOOST_TEST((!test("", eps(false))));
        BOOST_TEST((test("", !eps(false))));
    }

    {   // test lazy semantic predicate

        auto true_ = [](unused_type) { return true; };
        auto false_ = [](unused_type) { return false; };

        // cannot use lambda in constant expression before C++17
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(eps(std::true_type{}));
        BOOST_TEST((test("", eps(true_))));
        BOOST_TEST((!test("", eps(false_))));
        BOOST_TEST((test("", !eps(false_))));
    }

    return boost::report_errors();
}
