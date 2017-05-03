//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestResultOf
#include <boost/test/unit_test.hpp>

#include <boost/compute/function.hpp>
#include <boost/compute/functional/operator.hpp>
#include <boost/compute/type_traits/result_of.hpp>

BOOST_AUTO_TEST_CASE(result_of_function)
{
    using boost::compute::function;
    using boost::compute::result_of;

    BOOST_STATIC_ASSERT((
        boost::is_same<result_of<function<int()>()>::type, int>::value
    ));
}

BOOST_AUTO_TEST_CASE(result_of_operators)
{
    using boost::compute::plus;
    using boost::compute::minus;
    using boost::compute::result_of;

    BOOST_STATIC_ASSERT((
        boost::is_same<result_of<plus<int>(int, int)>::type, int>::value
    ));
    BOOST_STATIC_ASSERT((
        boost::is_same<result_of<minus<int>(int, int)>::type, int>::value
    ));
}
