// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/test/minimal.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


int test_main(int, char * [])
{
    {
        term<int> number = {{42}};

        auto fn = yap::make_expression_function(number);
        auto fn_copy = fn;

        BOOST_CHECK(fn() == 42);
        BOOST_CHECK(fn_copy() == 42);

        yap::value(number) = 21;

        BOOST_CHECK(fn() == 21);
        BOOST_CHECK(fn_copy() == 21);
    }

    {
        term<int> number = {{42}};

        auto fn = yap::make_expression_function(std::move(number));
        auto fn_copy = fn;

        BOOST_CHECK(fn() == 42);
        BOOST_CHECK(fn_copy() == 42);

        yap::value(number) = 21;

        BOOST_CHECK(fn() == 42);
        BOOST_CHECK(fn_copy() == 42);
    }

    {
        term<std::unique_ptr<int>> number = {
            {std::unique_ptr<int>(new int(42))}};

        auto fn = yap::make_expression_function(std::move(number));

        BOOST_CHECK(*fn() == 42);

        auto fn_2 = std::move(fn);
        BOOST_CHECK(*fn_2() == 42);

        yap::value(number) = std::unique_ptr<int>(new int(21));

        BOOST_CHECK(*fn_2() == 42);
    }

    return 0;
}
