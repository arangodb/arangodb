// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/arithmetic.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/core/lightweight_test.hpp>

template<class A, class B> void test()
{
    A a( 5 );
    BOOST_TEST_EQ( a.value(), 5 );

    B& b = a;
    BOOST_TEST_EQ( b.value(), 5 );

    b = 14;
    BOOST_TEST_EQ( b.value(), 14 );
    BOOST_TEST_EQ( a.value(), 14 );

    A const& ca = a;
    BOOST_TEST_EQ( ca.value(), 14 );

    B const& cb = b;
    BOOST_TEST_EQ( cb.value(), 14 );

    a = 31;

    BOOST_TEST_EQ( a.value(), 31 );
    BOOST_TEST_EQ( b.value(), 31 );

    BOOST_TEST_EQ( ca.value(), 31 );
    BOOST_TEST_EQ( cb.value(), 31 );
}

int main()
{
    using namespace boost::endian;

    test<big_int16_t, big_int16_buf_t>();
    test<little_int32_t, little_int32_buf_t>();

    return boost::report_errors();
}
