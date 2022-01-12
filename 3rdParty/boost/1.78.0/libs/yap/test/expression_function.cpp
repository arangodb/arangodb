// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/core/lightweight_test.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


int main()
{
    {
        term<int> number = {{42}};

        auto fn = yap::make_expression_function(number);
        auto fn_copy = fn;

        BOOST_TEST(fn() == 42);
        BOOST_TEST(fn_copy() == 42);

        yap::value(number) = 21;

        BOOST_TEST(fn() == 21);
        BOOST_TEST(fn_copy() == 21);
    }

    {
        term<int> number = {{42}};

        auto fn = yap::make_expression_function(std::move(number));
        auto fn_copy = fn;

        BOOST_TEST(fn() == 42);
        BOOST_TEST(fn_copy() == 42);

        yap::value(number) = 21;

        BOOST_TEST(fn() == 42);
        BOOST_TEST(fn_copy() == 42);
    }

    {
        term<std::unique_ptr<int>> number = {
            {std::unique_ptr<int>(new int(42))}};

        auto fn = yap::make_expression_function(std::move(number));

        BOOST_TEST(*fn() == 42);

        auto fn_2 = std::move(fn);
        BOOST_TEST(*fn_2() == 42);

        yap::value(number) = std::unique_ptr<int>(new int(21));

        BOOST_TEST(*fn_2() == 42);
    }

    return boost::report_errors();
}
