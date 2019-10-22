/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2014      John Fletcher
    Copyright (c) 2018      Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>

#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator/self.hpp>
#include <boost/phoenix/scope/lambda.hpp>

int main()
{
#if defined(RUNNING_ON_APPVEYOR) && BOOST_WORKAROUND(BOOST_MSVC, == 1800)
    // skip test
#else
    using boost::phoenix::lambda;
    using boost::phoenix::arg_names::_1;
    using boost::phoenix::local_names::_a;

    int x = 1;
    char const* y = "hello";
    int xx = 20;
    BOOST_TEST(lambda(_a = _1)[lambda[_a]](x)(y)(xx) == x);

    return boost::report_errors();
#endif
}
