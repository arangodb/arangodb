//  Copyright (c) 2001-2011 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/spirit/include/karma_duplicate.hpp>

#include <boost/spirit/include/karma.hpp>

#include <iostream>
#include "test.hpp"

using namespace spirit_test;

int main()
{
    using boost::spirit::karma::double_;
    using boost::spirit::karma::space;
    using boost::spirit::karma::duplicate;

    // test for sequences
    {
        BOOST_TEST(test("2.02.0", duplicate[double_ << double_], 2.0));
        BOOST_TEST(test_delimited("2.0 2.0 ",
            duplicate[double_ << double_], 2.0, space));
        BOOST_TEST(test("2.02.02.0",
            duplicate[double_ << double_ << double_], 2.0));
        BOOST_TEST(test_delimited("2.0 2.0 2.0 ",
            duplicate[double_ << double_ << double_], 2.0, space));
    }

    // test for non-sequences
    {
        BOOST_TEST(test("2.02.0", duplicate["2.0" << double_], 2.0));
        BOOST_TEST(test_delimited("2.0 2.0 ",
            duplicate["2.0" << double_], 2.0, space));
    }

    // test for subjects exposing no attribute
    {
        BOOST_TEST(test("2.02.0", duplicate["2.0"] << double_, 2.0));
        BOOST_TEST(test_delimited("2.0 2.0 ",
            duplicate["2.0"] << double_, 2.0, space));
    }

    // test for attribute reporting
    {
        BOOST_TEST(test("bar", (duplicate["bar"] | "foo")));
        BOOST_TEST(test("2.0", (duplicate[double_] | "foo"), 2.0));
        BOOST_TEST(test("2.02.0",
            (duplicate[double_ << double_] | "foo"), 2.0));
    }

    return boost::report_errors();
}
