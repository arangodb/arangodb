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

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(generate_uint)
{

    using boost::compute::uint_;

    boost::compute::threefry_engine<> rng(queue);

    boost::compute::vector<uint_> vector_ctr(20, context);

    uint32_t ctr[20];
    for(int i = 0; i < 10; i++) {
        ctr[i*2] = i;
        ctr[i*2+1] = 0;
    }

    boost::compute::copy(ctr, ctr+20, vector_ctr.begin(), queue);

    rng.generate(vector_ctr.begin(), vector_ctr.end(), queue);
    CHECK_RANGE_EQUAL(
        uint_, 20, vector_ctr,
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
         uint_(0x2d1fa53a),
         uint_(0xdfe8514c))
    );
}

BOOST_AUTO_TEST_SUITE_END()
