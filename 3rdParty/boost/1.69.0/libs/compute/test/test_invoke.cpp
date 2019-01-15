//---------------------------------------------------------------------------//
// Copyright (c) 2013-2015 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://kylelutz.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestInvoke
#include <boost/test/unit_test.hpp>

#include <boost/compute/function.hpp>
#include <boost/compute/lambda.hpp>
#include <boost/compute/functional/integer.hpp>
#include <boost/compute/functional/math.hpp>
#include <boost/compute/utility/invoke.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(invoke_builtin)
{
    BOOST_CHECK_EQUAL(compute::invoke(compute::abs<int>(), queue, -3), 3);
    BOOST_CHECK_CLOSE(compute::invoke(compute::pow<float>(), queue, 2.f, 8.f), 256.f, 1e-4);
}

BOOST_AUTO_TEST_CASE(invoke_function)
{
    BOOST_COMPUTE_FUNCTION(int, plus_two, (int x),
    {
        return x + 2;
    });

    BOOST_CHECK_EQUAL(compute::invoke(plus_two, queue, 2), 4);

    BOOST_COMPUTE_FUNCTION(float, get_max, (float x, float y),
    {
      if(x > y)
          return x;
      else
          return y;
    });

    BOOST_CHECK_EQUAL(compute::invoke(get_max, queue, 10.f, 20.f), 20.f);
}

BOOST_AUTO_TEST_CASE(invoke_lambda)
{
    using boost::compute::lambda::_1;
    using boost::compute::lambda::_2;

    BOOST_CHECK_EQUAL(compute::invoke(_1 / 2, queue, 4), 2);
    BOOST_CHECK_EQUAL(compute::invoke(_1 * _2 + 1, queue, 3, 3), 10);
}

BOOST_AUTO_TEST_SUITE_END()
