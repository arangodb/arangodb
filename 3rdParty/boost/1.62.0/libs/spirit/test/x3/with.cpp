/*=============================================================================
    Copyright (c) 2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include "test.hpp"

namespace x3 = boost::spirit::x3;

struct my_tag;

struct my_rule_class
{
    template <typename Iterator, typename Exception, typename Context>
    x3::error_handler_result
    on_error(Iterator&, Iterator const&, Exception const& x, Context const& context)
    {
        x3::get<my_tag>(context)++;
        return x3::error_handler_result::fail;
    }

    template <typename Iterator, typename Attribute, typename Context>
    inline void
    on_success(Iterator const&, Iterator const&, Attribute&, Context const& context)
    {
        x3::get<my_tag>(context)++;
    }
};

int
main()
{
    using spirit_test::test_attr;
    using spirit_test::test;

    using boost::spirit::x3::rule;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::with;

    { // injecting data into the context in the grammar

        int val = 0;
        auto r = rule<my_rule_class, char const*>() =
            '(' > int_ > ',' > int_ > ')'
            ;

        auto start =
            with<my_tag>(std::ref(val)) [ r ]
            ;

        BOOST_TEST(test("(123,456)", start));
        BOOST_TEST(!test("(abc,def)", start));
        BOOST_TEST(val == 2);
    }

    return boost::report_errors();
}
