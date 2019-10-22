//  Copyright Beman Dawes 2013
//  Copyright 2018 Peter Dimov

//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/detail/intrinsic.hpp>
#include <boost/core/lightweight_test.hpp>

typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

int main()
{
    std::cout << "BOOST_ENDIAN_INTRINSIC_MSG: " BOOST_ENDIAN_INTRINSIC_MSG << std::endl;

#ifndef BOOST_ENDIAN_NO_INTRINSICS

    uint16 x2 = 0x1122U;
    uint16 y2 = BOOST_ENDIAN_INTRINSIC_BYTE_SWAP_2(x2);
    BOOST_TEST_EQ( y2, 0x2211U );

    uint32 x4 = 0x11223344UL;
    uint32 y4 = BOOST_ENDIAN_INTRINSIC_BYTE_SWAP_4(x4);
    BOOST_TEST_EQ( y4, 0x44332211UL );

    uint64 x8 = 0x1122334455667788U;
    uint64 y8 = BOOST_ENDIAN_INTRINSIC_BYTE_SWAP_8(x8);
    BOOST_TEST_EQ( y8, 0x8877665544332211ULL );

#endif

    return boost::report_errors();
}
