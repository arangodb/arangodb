// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning( disable: 4309 ) // static_cast: truncation of constant value
#endif

#include <boost/endian/conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstddef>

template<class T, std::size_t N = sizeof(T)> struct test_value
{
};

template<class T> struct test_value<T, 1>
{
    static const T v1 = static_cast<T>( 0x1F );
    static const T w1 = static_cast<T>( 0x1F );

    static const T v2 = static_cast<T>( 0xF1 );
    static const T w2 = static_cast<T>( 0xF1 );
};

template<class T> T const test_value<T, 1>::v1;
template<class T> T const test_value<T, 1>::w1;
template<class T> T const test_value<T, 1>::v2;
template<class T> T const test_value<T, 1>::w2;

template<class T> struct test_value<T, 2>
{
    static const T v1 = static_cast<T>( 0x1F2E );
    static const T w1 = static_cast<T>( 0x2E1F );

    static const T v2 = static_cast<T>( 0xF1E2 );
    static const T w2 = static_cast<T>( 0xE2F1 );
};

template<class T> T const test_value<T, 2>::v1;
template<class T> T const test_value<T, 2>::w1;
template<class T> T const test_value<T, 2>::v2;
template<class T> T const test_value<T, 2>::w2;

template<class T> struct test_value<T, 4>
{
    static const T v1 = static_cast<T>( 0x1F2E3D4C );
    static const T w1 = static_cast<T>( 0x4C3D2E1F );

    static const T v2 = static_cast<T>( 0xF1E2D3C4 );
    static const T w2 = static_cast<T>( 0xC4D3E2F1 );
};

template<class T> T const test_value<T, 4>::v1;
template<class T> T const test_value<T, 4>::w1;
template<class T> T const test_value<T, 4>::v2;
template<class T> T const test_value<T, 4>::w2;

template<class T> struct test_value<T, 8>
{
    static const T v1 = static_cast<T>( 0x1F2E3D4C5B6A7988ull );
    static const T w1 = static_cast<T>( 0x88796A5B4C3D2E1Full );

    static const T v2 = static_cast<T>( 0xF1E2D3C4B5A69788ull );
    static const T w2 = static_cast<T>( 0x8897A6B5C4D3E2F1ull );
};

template<class T> T const test_value<T, 8>::v1;
template<class T> T const test_value<T, 8>::w1;
template<class T> T const test_value<T, 8>::v2;
template<class T> T const test_value<T, 8>::w2;

template<class T> void test()
{
    using boost::endian::endian_reverse;
    using boost::endian::endian_reverse_inplace;

    {
        T t1 = test_value<T>::v1;

        T t2 = endian_reverse( t1 );
        BOOST_TEST_EQ( t2, test_value<T>::w1 );

        T t3 = endian_reverse( t2 );
        BOOST_TEST_EQ( t3, t1 );

        T t4 = t1;

        endian_reverse_inplace( t4 );
        BOOST_TEST_EQ( t4, test_value<T>::w1 );

        endian_reverse_inplace( t4 );
        BOOST_TEST_EQ( t4, t1 );
    }

    {
        T t1 = test_value<T>::v2;

        T t2 = endian_reverse( t1 );
        BOOST_TEST_EQ( t2, test_value<T>::w2 );

        T t3 = endian_reverse( t2 );
        BOOST_TEST_EQ( t3, t1 );

        T t4 = t1;

        endian_reverse_inplace( t4 );
        BOOST_TEST_EQ( t4, test_value<T>::w2 );

        endian_reverse_inplace( t4 );
        BOOST_TEST_EQ( t4, t1 );
    }
}

int main()
{
    test<boost::int8_t>();
    test<boost::uint8_t>();

    test<boost::int16_t>();
    test<boost::uint16_t>();

    test<boost::int32_t>();
    test<boost::uint32_t>();

    test<boost::int64_t>();
    test<boost::uint64_t>();

    test<char>();
    test<unsigned char>();
    test<signed char>();

    test<short>();
    test<unsigned short>();

    test<int>();
    test<unsigned int>();

    test<long>();
    test<unsigned long>();

    test<long long>();
    test<unsigned long long>();

#if !defined(BOOST_NO_CXX11_CHAR16_T)
    test<char16_t>();
#endif

#if !defined(BOOST_NO_CXX11_CHAR32_T)
    test<char32_t>();
#endif

    return boost::report_errors();
}
