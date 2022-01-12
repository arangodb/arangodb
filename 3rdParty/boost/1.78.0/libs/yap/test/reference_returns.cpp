// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/mpl/assert.hpp>

#include <boost/core/lightweight_test.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

namespace yap = boost::yap;

namespace reference_returning {

    struct number
    {
        double value;
    };

    number a_result{3.0};
    number const the_result{13.0};

    number const & operator+(number a, number b) { return the_result; }

    number & operator-(number a, number b) { return a_result; }
}

int main()
{
    {
        term<reference_returning::number> unity = {{1.0}};
        auto plus_expr = unity + reference_returning::number{1.0};

        {
            reference_returning::number const & n = evaluate(plus_expr);
            BOOST_TEST(&n == &reference_returning::the_result);
        }

        using plus_eval_type = decltype(evaluate(plus_expr));
        BOOST_MPL_ASSERT((std::is_same<
                          plus_eval_type,
                          reference_returning::number const &>));

        auto minus_expr = unity - reference_returning::number{1.0};

        {
            reference_returning::number & n = evaluate(minus_expr);
            BOOST_TEST(&n == &reference_returning::a_result);
        }

        using minus_eval_type = decltype(evaluate(minus_expr));
        BOOST_MPL_ASSERT((std::is_same<
                          minus_eval_type,
                          reference_returning::number &>));

        using namespace yap::literals;

        {
            reference_returning::number & n =
                evaluate(1_p, reference_returning::a_result);
            BOOST_TEST(&n == &reference_returning::a_result);
        }

        using a_eval_type = decltype(evaluate(1_p, reference_returning::a_result));
        BOOST_MPL_ASSERT(
            (std::is_same<
                a_eval_type,
                reference_returning::number &>));

        {
            reference_returning::number const & n =
                evaluate(1_p, reference_returning::the_result);
            BOOST_TEST(&n == &reference_returning::the_result);
        }

        using the_eval_type = decltype(evaluate(1_p, reference_returning::the_result));
        BOOST_MPL_ASSERT(
            (std::is_same<
                the_eval_type,
                reference_returning::number const &>));
    }

    return boost::report_errors();
}
