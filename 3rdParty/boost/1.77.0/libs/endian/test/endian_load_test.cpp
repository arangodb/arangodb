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
    {
        unsigned char v[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

        // 1 -> 1

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int8_t, 1, boost::endian::order::little>( v )), 0x01 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint8_t, 1, boost::endian::order::little>( v )), 0x01 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int8_t, 1, boost::endian::order::big>( v )), 0x01 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint8_t, 1, boost::endian::order::big>( v )), 0x01 );

        // 1 -> 2

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int16_t, 1, boost::endian::order::little>( v )), 0x01 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint16_t, 1, boost::endian::order::little>( v )), 0x01 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int16_t, 1, boost::endian::order::big>( v )), 0x01 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint16_t, 1, boost::endian::order::big>( v )), 0x01 );

        // 2 -> 2

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int16_t, 2, boost::endian::order::little>( v )), 0x0201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint16_t, 2, boost::endian::order::little>( v )), 0x0201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int16_t, 2, boost::endian::order::big>( v )), 0x0102 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint16_t, 2, boost::endian::order::big>( v )), 0x0102 );

        // 1 -> 4

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 1, boost::endian::order::little>( v )), 0x01 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 1, boost::endian::order::little>( v )), 0x01 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 1, boost::endian::order::big>( v )), 0x01 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 1, boost::endian::order::big>( v )), 0x01 );

        // 2 -> 4

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 2, boost::endian::order::little>( v )), 0x0201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 2, boost::endian::order::little>( v )), 0x0201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 2, boost::endian::order::big>( v )), 0x0102 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 2, boost::endian::order::big>( v )), 0x0102 );

        // 3 -> 4

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 3, boost::endian::order::little>( v )), 0x030201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 3, boost::endian::order::little>( v )), 0x030201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 3, boost::endian::order::big>( v )), 0x010203 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 3, boost::endian::order::big>( v )), 0x010203 );

        // 4 -> 4

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 4, boost::endian::order::little>( v )), 0x04030201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 4, boost::endian::order::little>( v )), 0x04030201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 4, boost::endian::order::big>( v )), 0x01020304 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 4, boost::endian::order::big>( v )), 0x01020304 );

        // 1 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 1, boost::endian::order::little>( v )), 0x01 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 1, boost::endian::order::little>( v )), 0x01 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 1, boost::endian::order::big>( v )), 0x01 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 1, boost::endian::order::big>( v )), 0x01 );

        // 2 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 2, boost::endian::order::little>( v )), 0x0201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 2, boost::endian::order::little>( v )), 0x0201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 2, boost::endian::order::big>( v )), 0x0102 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 2, boost::endian::order::big>( v )), 0x0102 );

        // 3 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 3, boost::endian::order::little>( v )), 0x030201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 3, boost::endian::order::little>( v )), 0x030201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 3, boost::endian::order::big>( v )), 0x010203 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 3, boost::endian::order::big>( v )), 0x010203 );

        // 4 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 4, boost::endian::order::little>( v )), 0x04030201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 4, boost::endian::order::little>( v )), 0x04030201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 4, boost::endian::order::big>( v )), 0x01020304 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 4, boost::endian::order::big>( v )), 0x01020304 );

        // 5 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 5, boost::endian::order::little>( v )), 0x0504030201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 5, boost::endian::order::little>( v )), 0x0504030201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 5, boost::endian::order::big>( v )), 0x0102030405 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 5, boost::endian::order::big>( v )), 0x0102030405 );

        // 6 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 6, boost::endian::order::little>( v )), 0x060504030201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 6, boost::endian::order::little>( v )), 0x060504030201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 6, boost::endian::order::big>( v )), 0x010203040506 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 6, boost::endian::order::big>( v )), 0x010203040506 );

        // 7 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 7, boost::endian::order::little>( v )), 0x07060504030201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 7, boost::endian::order::little>( v )), 0x07060504030201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 7, boost::endian::order::big>( v )), 0x01020304050607 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 7, boost::endian::order::big>( v )), 0x01020304050607 );

        // 8 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 8, boost::endian::order::little>( v )), 0x0807060504030201 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 8, boost::endian::order::little>( v )), 0x0807060504030201 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 8, boost::endian::order::big>( v )), 0x0102030405060708 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 8, boost::endian::order::big>( v )), 0x0102030405060708 );
    }

    {
        unsigned char v[] = { 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8 };

        // 1 -> 1

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int8_t, 1, boost::endian::order::little>( v )), -15 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint8_t, 1, boost::endian::order::little>( v )), 0xF1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int8_t, 1, boost::endian::order::big>( v )), -15 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint8_t, 1, boost::endian::order::big>( v )), 0xF1 );

        // 1 -> 2

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int16_t, 1, boost::endian::order::little>( v )), -15 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint16_t, 1, boost::endian::order::little>( v )), 0xF1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int16_t, 1, boost::endian::order::big>( v )), -15 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint16_t, 1, boost::endian::order::big>( v )), 0xF1 );

        // 2 -> 2

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int16_t, 2, boost::endian::order::little>( v )), -3343 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint16_t, 2, boost::endian::order::little>( v )), 0xF2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int16_t, 2, boost::endian::order::big>( v )), -3598 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint16_t, 2, boost::endian::order::big>( v )), 0xF1F2 );

        // 1 -> 4

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 1, boost::endian::order::little>( v )), -15 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 1, boost::endian::order::little>( v )), 0xF1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 1, boost::endian::order::big>( v )), -15 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 1, boost::endian::order::big>( v )), 0xF1 );

        // 2 -> 4

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 2, boost::endian::order::little>( v )), -3343 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 2, boost::endian::order::little>( v )), 0xF2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 2, boost::endian::order::big>( v )), -3598 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 2, boost::endian::order::big>( v )), 0xF1F2 );

        // 3 -> 4

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 3, boost::endian::order::little>( v )), -789775 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 3, boost::endian::order::little>( v )), 0xF3F2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 3, boost::endian::order::big>( v )), -920845 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 3, boost::endian::order::big>( v )), 0xF1F2F3 );

        // 4 -> 4

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 4, boost::endian::order::little>( v )), 0xF4F3F2F1 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 4, boost::endian::order::little>( v )), 0xF4F3F2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int32_t, 4, boost::endian::order::big>( v )), 0xF1F2F3F4 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint32_t, 4, boost::endian::order::big>( v )), 0xF1F2F3F4 );

        // 1 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 1, boost::endian::order::little>( v )), -15 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 1, boost::endian::order::little>( v )), 0xF1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 1, boost::endian::order::big>( v )), -15 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 1, boost::endian::order::big>( v )), 0xF1 );

        // 2 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 2, boost::endian::order::little>( v )), -3343 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 2, boost::endian::order::little>( v )), 0xF2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 2, boost::endian::order::big>( v )), -3598 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 2, boost::endian::order::big>( v )), 0xF1F2 );

        // 3 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 3, boost::endian::order::little>( v )), -789775 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 3, boost::endian::order::little>( v )), 0xF3F2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 3, boost::endian::order::big>( v )), -920845 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 3, boost::endian::order::big>( v )), 0xF1F2F3 );

        // 4 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 4, boost::endian::order::little>( v )), -185339151 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 4, boost::endian::order::little>( v )), 0xF4F3F2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 4, boost::endian::order::big>( v )), -235736076 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 4, boost::endian::order::big>( v )), 0xF1F2F3F4 );

        // 5 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 5, boost::endian::order::little>( v )), -43135012111 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 5, boost::endian::order::little>( v )), 0xF5F4F3F2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 5, boost::endian::order::big>( v )), -60348435211 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 5, boost::endian::order::big>( v )), 0xF1F2F3F4F5 );

        // 6 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 6, boost::endian::order::little>( v )), -9938739662095 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 6, boost::endian::order::little>( v )), 0xF6F5F4F3F2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 6, boost::endian::order::big>( v )), -15449199413770 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 6, boost::endian::order::big>( v )), 0xF1F2F3F4F5F6 );

        // 7 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 7, boost::endian::order::little>( v )), -2261738553347343 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 7, boost::endian::order::little>( v )), 0xF7F6F5F4F3F2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 7, boost::endian::order::big>( v )), -3954995049924873 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 7, boost::endian::order::big>( v )), 0xF1F2F3F4F5F6F7 );

        // 8 -> 8

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 8, boost::endian::order::little>( v )), 0xF8F7F6F5F4F3F2F1 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 8, boost::endian::order::little>( v )), 0xF8F7F6F5F4F3F2F1 );

        BOOST_TEST_EQ( (boost::endian::endian_load<boost::int64_t, 8, boost::endian::order::big>( v )), 0xF1F2F3F4F5F6F7F8 );
        BOOST_TEST_EQ( (boost::endian::endian_load<boost::uint64_t, 8, boost::endian::order::big>( v )), 0xF1F2F3F4F5F6F7F8 );
    }

    return boost::report_errors();
}
