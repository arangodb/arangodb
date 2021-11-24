//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestAnyAllNoneOf
#include <boost/test/unit_test.hpp>

#include <limits>
#include <cmath>

#include <boost/compute/lambda.hpp>
#include <boost/compute/algorithm/all_of.hpp>
#include <boost/compute/algorithm/any_of.hpp>
#include <boost/compute/algorithm/none_of.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(any_all_none_of)
{
    int data[] = { 1, 2, 3, 4, 5, 6 };
    bc::vector<int> v(data, data + 6, queue);

    using ::boost::compute::_1;

    BOOST_CHECK(bc::any_of(v.begin(), v.end(), _1 == 6) == true);
    BOOST_CHECK(bc::any_of(v.begin(), v.end(), _1 == 9) == false);
    BOOST_CHECK(bc::none_of(v.begin(), v.end(), _1 == 6) == false);
    BOOST_CHECK(bc::none_of(v.begin(), v.end(), _1 == 9) == true);
    BOOST_CHECK(bc::all_of(v.begin(), v.end(), _1 == 6) == false);
    BOOST_CHECK(bc::all_of(v.begin(), v.end(), _1 < 9) == true);
    BOOST_CHECK(bc::all_of(v.begin(), v.end(), _1 < 6) == false);
    BOOST_CHECK(bc::all_of(v.begin(), v.end(), _1 >= 1) == true);
}

BOOST_AUTO_TEST_CASE(any_nan_inf)
{
    using ::boost::compute::_1;
    using ::boost::compute::lambda::isinf;
    using ::boost::compute::lambda::isnan;
    using ::boost::compute::lambda::isfinite;

    float nan = std::sqrt(-1.f);
    float inf = std::numeric_limits<float>::infinity();

    float data[] = { 1.2f, 2.3f, nan, nan, 3.4f, inf, 4.5f, inf };
    compute::vector<float> vector(data, data + 8, queue);

    BOOST_CHECK(compute::any_of(vector.begin(), vector.end(),
                                isinf(_1) || isnan(_1), queue) == true);
    BOOST_CHECK(compute::any_of(vector.begin(), vector.end(),
                                isfinite(_1), queue) == true);
    BOOST_CHECK(compute::all_of(vector.begin(), vector.end(),
                                isfinite(_1), queue) == false);
    BOOST_CHECK(compute::all_of(vector.begin(), vector.begin() + 2,
                                isfinite(_1), queue) == true);
    BOOST_CHECK(compute::all_of(vector.begin() + 2, vector.begin() + 4,
                                isnan(_1), queue) == true);
    BOOST_CHECK(compute::none_of(vector.begin(), vector.end(),
                                 isinf(_1), queue) == false);
    BOOST_CHECK(compute::none_of(vector.begin(), vector.begin() + 4,
                                 isinf(_1), queue) == true);
}

BOOST_AUTO_TEST_CASE(any_of_doctest)
{
    using boost::compute::lambda::_1;

    int data[] = { 1, 2, 3, 4 };
    boost::compute::vector<int> v(data, data + 4, queue);

    bool result =
//! [any_of]
boost::compute::any_of(v.begin(), v.end(), _1 < 0, queue);
//! [any_of]

    BOOST_CHECK(result == false);
}

BOOST_AUTO_TEST_SUITE_END()
