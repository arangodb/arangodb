// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if !defined(__GNUC__)

#include <boost/config/pragma_message.hpp>
BOOST_PRAGMA_MESSAGE( "Skipping test because __GNUC__ is not defined" )
int main() {}

#else

#define BOOST_ENDIAN_FORCE_PODNESS
#define BOOST_ENDIAN_NO_CTORS
#include <boost/endian/buffers.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::endian;

struct X
{
    big_uint16_buf_t a;
    native_float64_buf_t b;
    little_uint16_buf_t c;
} __attribute__((packed));

int main()
{
    BOOST_TEST_EQ( sizeof(big_uint16_buf_t), 2 );
    BOOST_TEST_EQ( sizeof(native_float64_buf_t), 8 );
    BOOST_TEST_EQ( sizeof(little_uint16_buf_t), 2 );

    BOOST_TEST_EQ( sizeof(X), 12 );

    return boost::report_errors();
}

#endif
