/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <string>
#include <sstream>

namespace x3 = boost::spirit::x3;

struct error_handler_base
{
    template <typename Iterator, typename Exception, typename Context>
    x3::error_handler_result on_error(
        Iterator& first, Iterator const& last
      , Exception const& x, Context const& context) const
    {
        std::string message = "Error! Expecting: " + x.which() + " here:";
        auto& error_handler = x3::get<x3::error_handler_tag>(context).get();
        error_handler(x.where(), message);
        return x3::error_handler_result::fail;
    }
};

struct test_rule_class : x3::annotate_on_success, error_handler_base {};

x3::rule<test_rule_class> const test_rule;
auto const test_rule_def = x3::lit("foo") > x3::lit("bar") > x3::lit("git");

BOOST_SPIRIT_DEFINE(test_rule);

void test(std::string const& line_break) {
    std::string const input("foo" + line_break + "  foo" + line_break + "git");
    auto const begin = std::begin(input);
    auto const end = std::end(input);

    std::stringstream stream;
    x3::error_handler<std::string::const_iterator> error_handler{begin, end, stream};

    auto const parser = x3::with<x3::error_handler_tag>(std::ref(error_handler))[test_rule];
    x3::phrase_parse(begin, end, parser, x3::space);

    BOOST_TEST_EQ(stream.str(), "In line 2:\nError! Expecting: \"bar\" here:\n  foo\n__^_\n");
}

int main() {
    test("\n");
    test("\r");
    test("\r\n");

    return boost::report_errors();
}
