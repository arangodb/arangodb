/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2014      John Fletcher
    Copyright (c) 2018      Kohei Takahsahi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <vector>

#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator/self.hpp>
#include <boost/phoenix/operator/arithmetic.hpp>
#include <boost/phoenix/scope/lambda.hpp>
#include <boost/phoenix/stl/algorithm/iteration.hpp>
#include <boost/phoenix/stl/container.hpp>

int main()
{
    using boost::phoenix::lambda;
    using boost::phoenix::arg_names::_1;
    using boost::phoenix::arg_names::_2;
    using boost::phoenix::local_names::_a;
    using boost::phoenix::for_each;

    int init[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    std::vector<int> v(init, init+10);

    int x = 0;
    for_each(_1, lambda(_a = _2)[_a += _1])(v, x);
    BOOST_TEST(x == 55);

    return boost::report_errors();
}

