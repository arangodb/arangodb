/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2014      John Fletcher
    Copyright (c) 2018      Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator/arithmetic.hpp>
#include <boost/phoenix/scope/lambda.hpp>

int main()
{
    using boost::phoenix::lambda;
    using boost::phoenix::arg_names::_1;

    int x = 1, y = 10;
    BOOST_TEST(
        (_1 + lambda[_1 + 2])(x)(y) == 1+10+2
    );

    return boost::report_errors();
}
