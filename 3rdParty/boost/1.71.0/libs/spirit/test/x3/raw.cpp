/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include <iostream>
#include <string>
#include "test.hpp"

int main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    using namespace boost::spirit::x3::ascii;
    using boost::spirit::x3::raw;
    using boost::spirit::x3::eps;
    using boost::spirit::x3::lit;
    using boost::spirit::x3::_attr;
    using boost::spirit::x3::parse;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::char_;

    {
        boost::iterator_range<char const*> range;
        std::string str;
        BOOST_TEST((test_attr("spirit_test_123", raw[alpha >> *(alnum | '_')], range)));
        BOOST_TEST((std::string(range.begin(), range.end()) == "spirit_test_123"));
        BOOST_TEST((test_attr("  spirit", raw[*alpha], range, space)));
        BOOST_TEST((range.size() == 6));
    }

    {
        std::string str;
        BOOST_TEST((test_attr("spirit_test_123", raw[alpha >> *(alnum | '_')], str)));
        BOOST_TEST((str == "spirit_test_123"));
    }

    {
        boost::iterator_range<char const*> range;
        BOOST_TEST((test("x", raw[alpha])));
        BOOST_TEST((test_attr("x", raw[alpha], range)));
    }

    {
        boost::iterator_range<char const*> range;
        BOOST_TEST((test("x", raw[alpha][ ([&](auto& ctx){ range = _attr(ctx); }) ])));
        BOOST_TEST(range.size() == 1 && *range.begin() == 'x');
    }

    {
        boost::iterator_range<char const*> range;
        BOOST_TEST((test("x123x", lit('x') >> raw[+digit] >> lit('x'))));
        BOOST_TEST((test_attr("x123x", lit('x') >> raw[+digit] >> lit('x'), range)));
        BOOST_TEST((std::string(range.begin(), range.end()) == "123"));
    }

    {
        using range = boost::iterator_range<std::string::iterator>;
        boost::variant<int, range> attr;

        std::string str("test");
        parse(str.begin(), str.end(),  (int_ | raw[*char_]), attr);

        auto rng = boost::get<range>(attr);
        BOOST_TEST(std::string(rng.begin(), rng.end()) == "test");
    }

    {
        std::vector<boost::iterator_range<std::string::iterator>> attr;
        std::string str("123abcd");
        parse(str.begin(), str.end()
          , (raw[int_] >> raw[*char_])
          , attr
        );
        BOOST_TEST(attr.size() == 2);
        BOOST_TEST(std::string(attr[0].begin(), attr[0].end()) == "123");
        BOOST_TEST(std::string(attr[1].begin(), attr[1].end()) == "abcd");
    }

    {
        std::pair<int, boost::iterator_range<std::string::iterator>> attr;
        std::string str("123abcd");
        parse(str.begin(), str.end()
          , (int_ >> raw[*char_])
          , attr
        );
        BOOST_TEST(attr.first == 123);
        BOOST_TEST(std::string(attr.second.begin(), attr.second.end()) == "abcd");
    }

    return boost::report_errors();
}
