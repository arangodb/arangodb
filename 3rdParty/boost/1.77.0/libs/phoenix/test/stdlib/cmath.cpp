/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2014      John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <cmath>
#include <boost/function.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/stl/cmath.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    double eps = 0.000001;
    using namespace boost::phoenix::arg_names;
    boost::function<bool(double, double)> f = boost::phoenix::fabs(_1 - _2) < eps;

    double x = boost::phoenix::pow(_1,_2)(2.,2.);
    double y = boost::phoenix::atan2(_1,_2)(1.,1.);
    double z = boost::phoenix::tan(_1)(y);

    BOOST_TEST(f(0.0, 0 * eps));
    BOOST_TEST(!f(0.0, eps));
    BOOST_TEST(std::fabs(x-4.) < eps );
    BOOST_TEST(std::fabs(z-1.) < eps );
    return boost::report_errors();
}
