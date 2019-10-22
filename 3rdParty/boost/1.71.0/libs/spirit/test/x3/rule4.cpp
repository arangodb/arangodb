/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/at.hpp>

#include <string>
#include <cstring>
#include <iostream>
#include "test.hpp"

namespace x3 = boost::spirit::x3;

int got_it = 0;

struct my_rule_class
{
    template <typename Iterator, typename Exception, typename Context>
    x3::error_handler_result
    on_error(Iterator&, Iterator const& last, Exception const& x, Context const&)
    {
        std::cout
            << "Error! Expecting: "
            << x.which()
            << ", got: \""
            << std::string(x.where(), last)
            << "\""
            << std::endl
            ;
        return x3::error_handler_result::fail;
    }

    template <typename Iterator, typename Attribute, typename Context>
    inline void
    on_success(Iterator const&, Iterator const&, Attribute&, Context const&)
    {
        ++got_it;
    }
};

int
main()
{
    using spirit_test::test_attr;
    using spirit_test::test;

    using namespace boost::spirit::x3::ascii;
    using boost::spirit::x3::rule;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::lit;

    { // show that ra = rb and ra %= rb works as expected
        rule<class a, int> ra;
        rule<class b, int> rb;
        int attr;

        auto ra_def = (ra %= int_);
        BOOST_TEST(test_attr("123", ra_def, attr));
        BOOST_TEST(attr == 123);

        auto rb_def = (rb %= ra_def);
        BOOST_TEST(test_attr("123", rb_def, attr));
        BOOST_TEST(attr == 123);

        auto rb_def2 = (rb = ra_def);
        BOOST_TEST(test_attr("123", rb_def2, attr));
        BOOST_TEST(attr == 123);
    }

    { // show that ra %= rb works as expected with semantic actions
        rule<class a, int> ra;
        rule<class b, int> rb;
        int attr;

        auto f = [](auto&){};
        auto ra_def = (ra %= int_[f]);
        BOOST_TEST(test_attr("123", ra_def, attr));
        BOOST_TEST(attr == 123);

        auto ra_def2 = (rb = (ra %= int_[f]));
        BOOST_TEST(test_attr("123", ra_def2, attr));
        BOOST_TEST(attr == 123);
    }


    { // std::string as container attribute with auto rules

        std::string attr;

        // test deduced auto rule behavior

        auto text = rule<class text, std::string>()
            = +(!char_(')') >> !char_('>') >> char_);

        attr.clear();
        BOOST_TEST(test_attr("x", text, attr));
        BOOST_TEST(attr == "x");
    }

    { // error handling

        auto r = rule<my_rule_class, char const*>()
            = '(' > int_ > ',' > int_ > ')';

        BOOST_TEST(test("(123,456)", r));
        BOOST_TEST(!test("(abc,def)", r));
        BOOST_TEST(!test("(123,456]", r));
        BOOST_TEST(!test("(123;456)", r));
        BOOST_TEST(!test("[123,456]", r));

        BOOST_TEST(got_it == 1);
    }

    {
        typedef boost::variant<double, int> v_type;
        auto r1 = rule<class r1, v_type>()
            = int_;
        v_type v;
        BOOST_TEST(test_attr("1", r1, v) && v.which() == 1 &&
            boost::get<int>(v) == 1);

        typedef boost::optional<int> ov_type;
        auto r2 = rule<class r2, ov_type>()
            = int_;
        ov_type ov;
        BOOST_TEST(test_attr("1", r2, ov) && ov && boost::get<int>(ov) == 1);
    }

    // test handling of single element fusion sequences
    {
        using boost::fusion::vector;
        using boost::fusion::at_c;
        auto r = rule<class r, vector<int>>()
            = int_;

        vector<int> v(0);
        BOOST_TEST(test_attr("1", r, v) && at_c<0>(v) == 1);
    }

    { // attribute compatibility test
        using boost::spirit::x3::rule;
        using boost::spirit::x3::int_;

        auto const expr = int_;

        short i;
        BOOST_TEST(test_attr("1", expr, i) && i == 1); // ok

        const rule< class int_rule, int > int_rule( "int_rule" );
        auto const int_rule_def = int_;
        auto const start  = int_rule = int_rule_def;

        short j;
        BOOST_TEST(test_attr("1", start, j) && j == 1); // error
    }

    return boost::report_errors();
}
