// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/arithmetic.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/core/lightweight_test.hpp>

template<class T> struct align
{
    char _;
    T v;

    explicit align( typename T::value_type y ): _(), v( y )
    {
    }
};

template<class T> struct align2
{
    char _;
    T v;
};

template<class T, class U> void test_buffer( U const & y, bool aligned )
{
    align<T> x( y );

    BOOST_TEST_EQ( sizeof(x), aligned? sizeof( align2<U> ): 1 + sizeof(U) );

    BOOST_TEST_EQ( x.v.value(), y );
}

template<class T, class U> void test_arithmetic( U const & y, bool aligned )
{
    test_buffer<T>( y, aligned );

    align<T> x( y );

    BOOST_TEST_EQ( x.v + 7, y + 7 );
}

int main()
{
    using namespace boost::endian;

    // buffers

    test_buffer<big_float32_buf_t>( 3.1416f, false );
    test_buffer<big_float64_buf_t>( 3.14159, false );

    test_buffer<little_float32_buf_t>( 3.1416f, false );
    test_buffer<little_float64_buf_t>( 3.14159, false );

    test_buffer<native_float32_buf_t>( 3.1416f, false );
    test_buffer<native_float64_buf_t>( 3.14159, false );

    test_buffer<big_float32_buf_at>( 3.1416f, true );
    test_buffer<big_float64_buf_at>( 3.14159, true );

    test_buffer<little_float32_buf_at>( 3.1416f, true );
    test_buffer<little_float64_buf_at>( 3.14159, true );

    // arithmetic

    test_arithmetic<big_float32_t>( 3.1416f, false );
    test_arithmetic<big_float64_t>( 3.14159, false );

    test_arithmetic<little_float32_t>( 3.1416f, false );
    test_arithmetic<little_float64_t>( 3.14159, false );

    test_arithmetic<native_float32_t>( 3.1416f, false );
    test_arithmetic<native_float64_t>( 3.14159, false );

    test_arithmetic<big_float32_at>( 3.1416f, true );
    test_arithmetic<big_float64_at>( 3.14159, true );

    test_arithmetic<little_float32_at>( 3.1416f, true );
    test_arithmetic<little_float64_at>( 3.14159, true );

    return boost::report_errors();
}
