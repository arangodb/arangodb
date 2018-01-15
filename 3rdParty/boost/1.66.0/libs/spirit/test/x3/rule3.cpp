/*=============================================================================
    Copyright (c) 2001-2012 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// this file deliberately contains non-ascii characters
// boostinspect:noascii

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include <string>
#include <cstring>
#include <iostream>
#include "test.hpp"

using boost::spirit::x3::_val;

struct f
{
    template <typename Context>
    void operator()(Context const& ctx) const
    {
        _val(ctx) += _attr(ctx);
    }
};

int main()
{
    using spirit_test::test_attr;
    using spirit_test::test;

    using namespace boost::spirit::x3::ascii;
    using boost::spirit::x3::rule;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::lit;


    { // synth attribute value-init

        std::string s;
        typedef rule<class r, std::string> rule_type;

        auto rdef = rule_type()
            = alpha                 [f()]
            ;

        BOOST_TEST(test_attr("abcdef", +rdef, s));
        BOOST_TEST(s == "abcdef");
    }

    { // synth attribute value-init

        std::string s;
        typedef rule<class r, std::string> rule_type;

        auto rdef = rule_type() =
            alpha /
               [](auto& ctx)
               {
                  _val(ctx) += _attr(ctx);
               }
            ;

        BOOST_TEST(test_attr("abcdef", +rdef, s));
        BOOST_TEST(s == "abcdef");
    }

    return boost::report_errors();
}
