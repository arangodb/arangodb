//---------------------------------------------------------------------------//
// Copyright (c) 2013 Muhammad Junaid Muzammil <mjunaidmuzammil@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://kylelutz.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestThreefry
#include <boost/test/unit_test.hpp>

#include <boost/compute/random/threefry_engine.hpp>
#include <boost/compute/container/vector.hpp>

#include <boost/compute/random/uniform_real_distribution.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(generate_uint)
{
    using boost::compute::uint_;

    boost::compute::threefry_engine<> random_engine(queue);
    boost::compute::vector<uint_> random_values(19, context);

    random_engine.generate(random_values.begin(), random_values.end(), queue);
    queue.finish();
    CHECK_RANGE_EQUAL(
        uint_, 19, random_values,
        (uint_(0x6b200159),
         uint_(0x99ba4efe),
         uint_(0x508efb2c),
         uint_(0xc0de3f32),
         uint_(0x64a626ec),
         uint_(0xfc15e573),
         uint_(0xb8abc4d1),
         uint_(0x537eb86),
         uint_(0xac6dc2bb),
         uint_(0xa7adb3c3),
         uint_(0x5641e094),
         uint_(0xe4ab4fd),
         uint_(0xa53c1ce9),
         uint_(0xabcf1dba),
         uint_(0x2677a25a),
         uint_(0x76cf5efc),
         uint_(0x2d08247f),
         uint_(0x815480f1),
         uint_(0x2d1fa53a))
    );
}

BOOST_AUTO_TEST_CASE(generate_float)
{
    using boost::compute::float_;

    boost::compute::threefry_engine<> random_engine(queue);
    boost::compute::uniform_real_distribution<float_> random_distribution(0.f, 4.f);

    boost::compute::vector<float_> random_values(1024, context);
    random_distribution.generate(
        random_values.begin(), random_values.end(), random_engine, queue
    );

    std::vector<float_> random_values_host(1024);
    boost::compute::copy(
        random_values.begin(), random_values.end(),
        random_values_host.begin(),
        queue
    );
    queue.finish();

    double sum = 0.0;
    for(size_t i = 0; i < random_values_host.size(); i++)
    {
        BOOST_CHECK_LT(random_values_host[i], 4.0f);
        BOOST_CHECK_GE(random_values_host[i], 0.0f);
        sum += random_values_host[i];
    }
    double mean = sum / random_values_host.size();
    // For 1024 it can be 10% off
    BOOST_CHECK_CLOSE(mean, 2.0f, 10.0);
}

BOOST_AUTO_TEST_SUITE_END()
