/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>

#include <boost/fusion/include/std_pair.hpp>
#include <vector>

#include "test.hpp"

int main()
{
    using spirit_test::test_attr;
    using boost::spirit::x3::attr;
    using boost::spirit::x3::int_;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(attr);
    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(attr(1));
    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(attr("asd"));
    {
        constexpr char s[] = "asd";
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(attr(s));
    }

    {
        int d = 0;
        BOOST_TEST(test_attr("", attr(1), d) && d == 1);

        int d1 = 1;
        BOOST_TEST(test_attr("", attr(d1), d) && d == 1);

        std::pair<int, int> p;
        BOOST_TEST(test_attr("1", int_ >> attr(1), p) &&
            p.first == 1 && p.second == 1);

        char c = '\0';
        BOOST_TEST(test_attr("", attr('a'), c) && c == 'a');

        // $$$ Needs some special is_convertible support, or
        // str ends up with an explicit null-terminator... $$$
        std::string str;
        //~ BOOST_TEST(test_attr("", attr("test"), str) && str == "test");

        str.clear();
        BOOST_TEST(test_attr("", attr(std::string("test")), str))
          && BOOST_TEST_EQ(str, "test");

        int array[] = {0, 1, 2};
        std::vector<int> vec;
        BOOST_TEST(test_attr("", attr(array), vec) && vec.size() == 3 &&
            vec[0] == 0 && vec[1] == 1 && vec[2] == 2);
    }

    {
        std::string s;
        BOOST_TEST(test_attr("s", "s" >> attr(std::string("123")), s) &&
            s == "123");

        s.clear();
        BOOST_TEST(test_attr("", attr(std::string("123")) >> attr(std::string("456")), s))
          && BOOST_TEST_EQ(s, "123456");
    }

    return boost::report_errors();
}
