/*=============================================================================
    Copyright (c) 2001-2012 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

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


struct stationary : boost::noncopyable
{
    explicit stationary(int i) : val{i} {}
    stationary& operator=(int i) { val = i; return *this; }

    int val;
};


namespace check_stationary {

boost::spirit::x3::rule<class a_r, stationary> const a;
boost::spirit::x3::rule<class b_r, stationary> const b;

auto const a_def = '{' >> boost::spirit::x3::int_ >> '}';
auto const b_def = a;

BOOST_SPIRIT_DEFINE(a, b)

}


int main()
{
    using spirit_test::test_attr;
    using spirit_test::test;

    using namespace boost::spirit::x3::ascii;
    using boost::spirit::x3::rule;
    using boost::spirit::x3::lit;
    using boost::spirit::x3::eps;
    using boost::spirit::x3::unused_type;


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

    {
        auto r = rule<class r, int>{} = eps[([] (auto& ctx) {
            using boost::spirit::x3::_val;
            static_assert(std::is_same<std::decay_t<decltype(_val(ctx))>, unused_type>::value,
                "Attribute must not be synthesized");
        })];
        BOOST_TEST(test("", r));
    }

    { // ensure no unneded synthesization, copying and moving occured
        stationary st { 0 };
        BOOST_TEST(test_attr("{42}", check_stationary::b, st));
        BOOST_TEST_EQ(st.val, 42);
    }

    return boost::report_errors();
}
