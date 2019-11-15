//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestTypes
#include <boost/test/unit_test.hpp>

#include <string>
#include <sstream>

#include <boost/compute/types/fundamental.hpp>

BOOST_AUTO_TEST_CASE(vector_ctor)
{
    boost::compute::int4_ i4(1, 2, 3, 4);
    BOOST_CHECK(i4 == boost::compute::int4_(1, 2, 3, 4));
    BOOST_CHECK_EQUAL(i4, boost::compute::int4_(1, 2, 3, 4));
    BOOST_CHECK_EQUAL(i4[0], 1);
    BOOST_CHECK_EQUAL(i4[1], 2);
    BOOST_CHECK_EQUAL(i4[2], 3);
    BOOST_CHECK_EQUAL(i4[3], 4);

    i4 = boost::compute::int4_(1);
    BOOST_CHECK(i4 == boost::compute::int4_(1, 1, 1, 1));
    BOOST_CHECK(i4 == (boost::compute::vector_type<int, size_t(4)>(1)));
    BOOST_CHECK_EQUAL(i4, boost::compute::int4_(1, 1, 1, 1));
    BOOST_CHECK_EQUAL(i4[0], 1);
    BOOST_CHECK_EQUAL(i4[1], 1);
    BOOST_CHECK_EQUAL(i4[2], 1);
    BOOST_CHECK_EQUAL(i4[3], 1);
}

BOOST_AUTO_TEST_CASE(vector_string)
{
    std::stringstream stream;
    stream << boost::compute::int2_(1, 2);
    BOOST_CHECK_EQUAL(stream.str(), std::string("int2(1, 2)"));
}

BOOST_AUTO_TEST_CASE(vector_accessors_basic)
{
    boost::compute::float4_ v;
    v.x = 1;
    v.y = 2;
    v.z = 3;
    v.w = 4;
    BOOST_CHECK(v == boost::compute::float4_(1, 2, 3, 4));
}

BOOST_AUTO_TEST_CASE(vector_accessors_all)
{
    boost::compute::int2_ i2(1, 2);
    BOOST_CHECK_EQUAL(i2.x, 1);
    BOOST_CHECK_EQUAL(i2.y, 2);

    boost::compute::int4_ i4(1, 2, 3, 4);
    BOOST_CHECK_EQUAL(i4.x, 1);
    BOOST_CHECK_EQUAL(i4.y, 2);
    BOOST_CHECK_EQUAL(i4.z, 3);
    BOOST_CHECK_EQUAL(i4.w, 4);

    boost::compute::int8_ i8(1, 2, 3, 4, 5, 6, 7, 8);
    BOOST_CHECK_EQUAL(i8.s0, 1);
    BOOST_CHECK_EQUAL(i8.s1, 2);
    BOOST_CHECK_EQUAL(i8.s2, 3);
    BOOST_CHECK_EQUAL(i8.s3, 4);
    BOOST_CHECK_EQUAL(i8.s4, 5);
    BOOST_CHECK_EQUAL(i8.s5, 6);
    BOOST_CHECK_EQUAL(i8.s6, 7);
    BOOST_CHECK_EQUAL(i8.s7, 8);

    boost::compute::int16_ i16(
        1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16);
    BOOST_CHECK_EQUAL(i16.s0, 1);
    BOOST_CHECK_EQUAL(i16.s1, 2);
    BOOST_CHECK_EQUAL(i16.s2, 3);
    BOOST_CHECK_EQUAL(i16.s3, 4);
    BOOST_CHECK_EQUAL(i16.s4, 5);
    BOOST_CHECK_EQUAL(i16.s5, 6);
    BOOST_CHECK_EQUAL(i16.s6, 7);
    BOOST_CHECK_EQUAL(i16.s7, 8);
    BOOST_CHECK_EQUAL(i16.s8, 9);
    BOOST_CHECK_EQUAL(i16.s9, 10);
    BOOST_CHECK_EQUAL(i16.sa, 11);
    BOOST_CHECK_EQUAL(i16.sb, 12);
    BOOST_CHECK_EQUAL(i16.sc, 13);
    BOOST_CHECK_EQUAL(i16.sd, 14);
    BOOST_CHECK_EQUAL(i16.se, 15);
    BOOST_CHECK_EQUAL(i16.sf, 16);
}
