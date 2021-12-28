/*=============================================================================
    Copyright (c) 2009 Chris Hoeppler
    Copyright (c) 2014 Lee Clagett

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>

#include <boost/spirit/home/x3/char.hpp>
#include <boost/spirit/home/x3/core.hpp>
#include <boost/spirit/home/x3/numeric.hpp>
#include <boost/spirit/home/x3/operator.hpp>
#include <boost/spirit/home/x3/string.hpp>
#include <boost/spirit/home/x3/directive/confix.hpp>

#include "test.hpp"

int main()
{
    namespace x3 = boost::spirit::x3;
    using namespace spirit_test;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(x3::confix('(', ')'));
    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(x3::confix("[", "]"));
    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(x3::confix("/*", "*/"));

    {
        const auto comment = x3::confix("/*", "*/");

        BOOST_TEST(test_failure("/abcdef*/", comment["abcdef"]));
        BOOST_TEST(test_failure("/* abcdef*/", comment["abcdef"]));
        BOOST_TEST(test_failure("/*abcdef */", comment["abcdef"]));
        BOOST_TEST(test("/*abcdef*/", comment["abcdef"]));

        {
            unsigned value = 0;
            BOOST_TEST(
                test_attr(" /* 123 */ ", comment[x3::uint_], value, x3::space));
            BOOST_TEST(value == 123);

            using x3::_attr;
            value = 0;
            const auto lambda = [&value](auto& ctx ){ value = _attr(ctx) + 1; };
            BOOST_TEST(test_attr("/*123*/", comment[x3::uint_][lambda], value));
            BOOST_TEST(value == 124);
        }
    }
    {
        const auto array = x3::confix('[', ']');

        {
            std::vector<unsigned> values;

            BOOST_TEST(test("[0,2,4,6,8]", array[x3::uint_ % ',']));
            BOOST_TEST(test_attr("[0,2,4,6,8]", array[x3::uint_ % ','], values));
            BOOST_TEST(
                values.size() == 5 &&
                values[0] == 0 &&
                values[1] == 2 &&
                values[2] == 4 &&
                values[3] == 6 &&
                values[4] == 8);
        }
        {
            std::vector<std::vector<unsigned>> values;
            BOOST_TEST(
                test("[[1,3,5],[0,2,4]]", array[array[x3::uint_ % ','] % ',']));
            BOOST_TEST(
                test_attr(
                    "[[1,3,5],[0,2,4]]",
                    array[array[x3::uint_ % ','] % ','],
                    values));
            BOOST_TEST(
                values.size() == 2 &&
                values[0].size() == 3 &&
                values[0][0] == 1 &&
                values[0][1] == 3 &&
                values[0][2] == 5 &&
                values[1].size() == 3 &&
                values[1][0] == 0 &&
                values[1][1] == 2 &&
                values[1][2] == 4);
        }
    }

    return boost::report_errors();
}
