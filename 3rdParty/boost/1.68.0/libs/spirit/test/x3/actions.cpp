/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <cstring>
#include <functional>

namespace x3 = boost::spirit::x3;

int x = 0;

auto fun1 =
    [](auto& ctx)
    {
        x += x3::_attr(ctx);
    }
;

struct fun_action
{
    template <typename Context>
    void operator()(Context const& ctx) const
    {
        x += x3::_attr(ctx);
    }
};

auto fail =
    [](auto& ctx)
    {
        x3::_pass(ctx) = false;
    }
;

struct setnext
{
    setnext(char& next) : next(next) {}

    template <typename Context>
    void operator()(Context const& ctx) const
    {
        next = x3::_attr(ctx);
    }

    char& next;
};

int main()
{
    using x3::int_;

    {
        char const *s1 = "{42}", *e1 = s1 + std::strlen(s1);
        x3::parse(s1, e1, '{' >> int_[fun1] >> '}');
    }


    {
        char const *s1 = "{42}", *e1 = s1 + std::strlen(s1);
        x3::parse(s1, e1, '{' >> int_[fun_action()] >> '}');
    }

    {
        using namespace std::placeholders;
        char const *s1 = "{42}", *e1 = s1 + std::strlen(s1);
        x3::parse(s1, e1, '{' >> int_[std::bind(fun_action(), _1)] >> '}');
    }

    BOOST_TEST(x == (42*3));

    {
       std::string input("1234 6543");
       char next = '\0';
       BOOST_TEST(x3::phrase_parse(input.begin(), input.end(),
          x3::int_[fail] | x3::digit[setnext(next)], x3::space));
       BOOST_TEST(next == '1');
    }

    return boost::report_errors();
}
