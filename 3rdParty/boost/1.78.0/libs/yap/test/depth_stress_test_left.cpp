// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/core/lightweight_test.hpp>

#include <sstream>


template <typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

namespace yap = boost::yap;


int main()
{
{
    term<double> unity{1.0};

    {
        auto expr = (unity + (unity + (unity + (unity +    (unity + (unity + (unity + unity)))))));
        double result = boost::yap::evaluate(expr);
        BOOST_TEST(result == 8.0);
    }

    {
        auto expr =
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +

            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));
        double result = boost::yap::evaluate(expr);
        BOOST_TEST(result == 64.0);
    }

#if 0 // This makes the Travis VMs barf.
    {
        auto expr =
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +

            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity +
            (unity + (unity + (unity + (unity +    (unity + (unity + (unity + (unity))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));
        double result = boost::yap::evaluate(
            (expr + (expr + (expr + (expr +    (expr + (expr + (expr + (expr +
            (expr + (expr + (expr + (expr +    (expr + (expr + (expr + (expr +
            (expr + (expr + (expr + (expr +    (expr + (expr + (expr + (expr +
            (expr + (expr + (expr + (expr +    (expr + (expr + (expr + (expr))))))))))))))))))))))))))))))))
        );
        BOOST_TEST(result == 64.0 * 32.0);
    }
#endif
}

return boost::report_errors();
}
