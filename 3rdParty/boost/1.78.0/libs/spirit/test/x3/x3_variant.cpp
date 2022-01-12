/*=============================================================================
    Copyright (c) 2001-2016 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

#include <string>
#include <iostream>
#include "test.hpp"

namespace x3 = boost::spirit::x3;

struct none {};

using variant = x3::variant<
        none
      , bool
      , std::string
      , int
      , double
    >;

struct ast : variant
{
    using variant::variant;
    using variant::operator=;

    ast(char const* s)
      : variant(std::string{s})
    {}

    ast& operator=(char const* s)
    {
        variant::operator=(std::string{s});
        return *this;
    }
};

int
main()
{
    {
        ast v{123};
        BOOST_TEST(boost::get<int>(v) == 123);

        v = "test";
        BOOST_TEST(boost::get<std::string>(v) == "test");

        v = true;
        BOOST_TEST(boost::get<bool>(v) == true);
    }
    return boost::report_errors();
}
