// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>

int main()
{
    using namespace boost::endian;

    unsigned char v[] = { 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8 };

    // 16

    BOOST_TEST_EQ( load_little_s16( v ), -3343 );
    BOOST_TEST_EQ( load_little_u16( v ), 0xF2F1 );

    BOOST_TEST_EQ( load_big_s16( v ), -3598 );
    BOOST_TEST_EQ( load_big_u16( v ), 0xF1F2 );

    // 24

    BOOST_TEST_EQ( load_little_s24( v ), -789775 );
    BOOST_TEST_EQ( load_little_u24( v ), 0xF3F2F1 );

    BOOST_TEST_EQ( load_big_s24( v ), -920845 );
    BOOST_TEST_EQ( load_big_u24( v ), 0xF1F2F3 );

    // 32

    BOOST_TEST_EQ( load_little_s32( v ), 0xF4F3F2F1 );
    BOOST_TEST_EQ( load_little_u32( v ), 0xF4F3F2F1 );

    BOOST_TEST_EQ( load_big_s32( v ), 0xF1F2F3F4 );
    BOOST_TEST_EQ( load_big_u32( v ), 0xF1F2F3F4 );

    // 40

    BOOST_TEST_EQ( load_little_s40( v ), -43135012111 );
    BOOST_TEST_EQ( load_little_u40( v ), 0xF5F4F3F2F1 );

    BOOST_TEST_EQ( load_big_s40( v ), -60348435211 );
    BOOST_TEST_EQ( load_big_u40( v ), 0xF1F2F3F4F5 );

    // 48

    BOOST_TEST_EQ( load_little_s48( v ), -9938739662095 );
    BOOST_TEST_EQ( load_little_u48( v ), 0xF6F5F4F3F2F1 );

    BOOST_TEST_EQ( load_big_s48( v ), -15449199413770 );
    BOOST_TEST_EQ( load_big_u48( v ), 0xF1F2F3F4F5F6 );

    // 56

    BOOST_TEST_EQ( load_little_s56( v ), -2261738553347343 );
    BOOST_TEST_EQ( load_little_u56( v ), 0xF7F6F5F4F3F2F1 );

    BOOST_TEST_EQ( load_big_s56( v ), -3954995049924873 );
    BOOST_TEST_EQ( load_big_u56( v ), 0xF1F2F3F4F5F6F7 );

    // 64

    BOOST_TEST_EQ( load_little_s64( v ), 0xF8F7F6F5F4F3F2F1 );
    BOOST_TEST_EQ( load_little_u64( v ), 0xF8F7F6F5F4F3F2F1 );

    BOOST_TEST_EQ( load_big_s64( v ), 0xF1F2F3F4F5F6F7F8 );
    BOOST_TEST_EQ( load_big_u64( v ), 0xF1F2F3F4F5F6F7F8 );

    return boost::report_errors();
}
