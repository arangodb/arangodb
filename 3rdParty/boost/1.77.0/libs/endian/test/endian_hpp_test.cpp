//  Copyright 2019 Peter Dimov
//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    using namespace boost::endian;

    // conversion

    {
        BOOST_TEST_EQ( endian_reverse( 0x01020304 ), 0x04030201 );
    }

    // buffers

    {
        little_uint32_buf_t v( 0x01020304 );

        BOOST_TEST_EQ( v.data()[ 0 ], 0x04 );
        BOOST_TEST_EQ( v.data()[ 1 ], 0x03 );
        BOOST_TEST_EQ( v.data()[ 2 ], 0x02 );
        BOOST_TEST_EQ( v.data()[ 3 ], 0x01 );
    }

    {
        big_uint32_buf_t v( 0x01020304 );

        BOOST_TEST_EQ( v.data()[ 0 ], 0x01 );
        BOOST_TEST_EQ( v.data()[ 1 ], 0x02 );
        BOOST_TEST_EQ( v.data()[ 2 ], 0x03 );
        BOOST_TEST_EQ( v.data()[ 3 ], 0x04 );
    }

    // arithmetic

    {
        little_uint32_t v( 0x01020304 );

        BOOST_TEST_EQ( v.data()[ 0 ], 0x04 );
        BOOST_TEST_EQ( v.data()[ 1 ], 0x03 );
        BOOST_TEST_EQ( v.data()[ 2 ], 0x02 );
        BOOST_TEST_EQ( v.data()[ 3 ], 0x01 );
    }

    {
        big_uint32_t v( 0x01020304 );

        BOOST_TEST_EQ( v.data()[ 0 ], 0x01 );
        BOOST_TEST_EQ( v.data()[ 1 ], 0x02 );
        BOOST_TEST_EQ( v.data()[ 2 ], 0x03 );
        BOOST_TEST_EQ( v.data()[ 3 ], 0x04 );
    }

    return boost::report_errors();
}
