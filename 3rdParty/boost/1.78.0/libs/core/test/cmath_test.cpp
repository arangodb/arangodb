// Test for boost/core/cmath.hpp
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/cmath.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <limits>
#include <cfloat>

template<class T> void test_positive_normal( T x )
{
    BOOST_TEST( boost::core::isfinite( x ) );
    BOOST_TEST( !boost::core::isinf( x ) );
    BOOST_TEST( !boost::core::isnan( x ) );
    BOOST_TEST( boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_normal );

    BOOST_TEST( !boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(+2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(+2) );
}

template<class T> void test_negative_normal( T x )
{
    BOOST_TEST( boost::core::isfinite( x ) );
    BOOST_TEST( !boost::core::isinf( x ) );
    BOOST_TEST( !boost::core::isnan( x ) );
    BOOST_TEST( boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_normal );

    BOOST_TEST( boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(-2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(-2) );
}

template<class T> void test_positive_zero( T x )
{
    BOOST_TEST( boost::core::isfinite( x ) );
    BOOST_TEST( !boost::core::isinf( x ) );
    BOOST_TEST( !boost::core::isnan( x ) );
    BOOST_TEST( !boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_zero );

    BOOST_TEST( !boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(+2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(+2) );
}

template<class T> void test_negative_zero( T x )
{
    BOOST_TEST( boost::core::isfinite( x ) );
    BOOST_TEST( !boost::core::isinf( x ) );
    BOOST_TEST( !boost::core::isnan( x ) );
    BOOST_TEST( !boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_zero );

#if defined(BOOST_CORE_USE_GENERIC_CMATH) && BOOST_WORKAROUND(BOOST_GCC, < 40700)

    // g++ 4.4, 4.6 fail these tests with optimizations on

#else

    BOOST_TEST( boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(-2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(-2) );

#endif
}

template<class T> void test_positive_infinity( T x )
{
    BOOST_TEST( !boost::core::isfinite( x ) );
    BOOST_TEST( boost::core::isinf( x ) );
    BOOST_TEST( !boost::core::isnan( x ) );
    BOOST_TEST( !boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_infinite );

    BOOST_TEST( !boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(+2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(+2) );
}

template<class T> void test_negative_infinity( T x )
{
    BOOST_TEST( !boost::core::isfinite( x ) );
    BOOST_TEST( boost::core::isinf( x ) );
    BOOST_TEST( !boost::core::isnan( x ) );
    BOOST_TEST( !boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_infinite );

    BOOST_TEST( boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(-2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(-2) );
}

template<class T> void test_positive_nan( T x )
{
    BOOST_TEST( !boost::core::isfinite( x ) );
    BOOST_TEST( !boost::core::isinf( x ) );
    BOOST_TEST( boost::core::isnan( x ) );
    BOOST_TEST( !boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_nan );

    BOOST_TEST( !boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(+2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(+2) );
}

template<class T> void test_negative_nan( T x )
{
    BOOST_TEST( !boost::core::isfinite( x ) );
    BOOST_TEST( !boost::core::isinf( x ) );
    BOOST_TEST( boost::core::isnan( x ) );
    BOOST_TEST( !boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_nan );

    BOOST_TEST( boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(-2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(-2) );
}

template<class T> void test_positive_subnormal( T x )
{
    BOOST_TEST( boost::core::isfinite( x ) );
    BOOST_TEST( !boost::core::isinf( x ) );
    BOOST_TEST( !boost::core::isnan( x ) );
    BOOST_TEST( !boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_subnormal );

    BOOST_TEST( !boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(+2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(+2) );
}

template<class T> void test_negative_subnormal( T x )
{
    BOOST_TEST( boost::core::isfinite( x ) );
    BOOST_TEST( !boost::core::isinf( x ) );
    BOOST_TEST( !boost::core::isnan( x ) );
    BOOST_TEST( !boost::core::isnormal( x ) );

    BOOST_TEST_EQ( boost::core::fpclassify( x ), boost::core::fp_subnormal );

    BOOST_TEST( boost::core::signbit( x ) );

    BOOST_TEST_EQ( boost::core::copysign( T(+2), x ), T(-2) );
    BOOST_TEST_EQ( boost::core::copysign( T(-2), x ), T(-2) );
}

template<class T> void test_positive_normal_( T x )
{
    test_positive_normal( x );
    test_positive_normal( boost::core::copysign( x, T(+1) ) );
    test_negative_normal( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_negative_normal_( T x )
{
    test_negative_normal( x );
    test_positive_normal( boost::core::copysign( x, T(+1) ) );
    test_negative_normal( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_positive_zero_( T x )
{
    test_positive_zero( x );
    test_positive_zero( boost::core::copysign( x, T(+1) ) );
    test_negative_zero( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_negative_zero_( T x )
{
    test_negative_zero( x );
    test_positive_zero( boost::core::copysign( x, T(+1) ) );
    test_negative_zero( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_positive_infinity_( T x )
{
    test_positive_infinity( x );
    test_positive_infinity( boost::core::copysign( x, T(+1) ) );
    test_negative_infinity( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_negative_infinity_( T x )
{
    test_negative_infinity( x );
    test_positive_infinity( boost::core::copysign( x, T(+1) ) );
    test_negative_infinity( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_positive_nan_( T x )
{
    test_positive_nan( x );
    test_positive_nan( boost::core::copysign( x, T(+1) ) );
    test_negative_nan( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_negative_nan_( T x )
{
    test_negative_nan( x );
    test_positive_nan( boost::core::copysign( x, T(+1) ) );
    test_negative_nan( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_positive_subnormal_( T x )
{
    test_positive_subnormal( x );
    test_positive_subnormal( boost::core::copysign( x, T(+1) ) );
    test_negative_subnormal( boost::core::copysign( x, T(-1) ) );
}

template<class T> void test_negative_subnormal_( T x )
{
    test_negative_subnormal( x );
    test_positive_subnormal( boost::core::copysign( x, T(+1) ) );
    test_negative_subnormal( boost::core::copysign( x, T(-1) ) );
}

int main()
{
    // float

    test_positive_normal_( +1.0f );
    test_negative_normal_( -1.0f );
    test_positive_normal_( FLT_MIN );
    test_negative_normal_( -FLT_MIN );
    test_positive_normal_( FLT_MAX );
    test_negative_normal_( -FLT_MAX );
    test_positive_zero_( +0.0f );
    test_negative_zero_( -0.0f );
    test_positive_infinity_( std::numeric_limits<float>::infinity() );
    test_negative_infinity_( -std::numeric_limits<float>::infinity() );
    test_positive_nan_( std::numeric_limits<float>::quiet_NaN() );
    test_negative_nan_( boost::core::copysign( std::numeric_limits<float>::quiet_NaN(), -1.0f ) );
    test_positive_subnormal_( FLT_MIN / 2 );
    test_negative_subnormal_( -FLT_MIN / 2 );

    // double

    test_positive_normal_( +1.0 );
    test_negative_normal_( -1.0 );
    test_positive_normal_( DBL_MIN );
    test_negative_normal_( -DBL_MIN );
    test_positive_normal_( DBL_MAX );
    test_negative_normal_( -DBL_MAX );
    test_positive_zero_( +0.0 );
    test_negative_zero_( -0.0 );
    test_positive_infinity_( std::numeric_limits<double>::infinity() );
    test_negative_infinity_( -std::numeric_limits<double>::infinity() );
    test_positive_nan_( std::numeric_limits<double>::quiet_NaN() );
    test_negative_nan_( boost::core::copysign( std::numeric_limits<double>::quiet_NaN(), -1.0 ) );
    test_positive_subnormal_( DBL_MIN / 2 );
    test_negative_subnormal_( -DBL_MIN / 2 );

    // long double

    test_positive_normal_( +1.0l );
    test_negative_normal_( -1.0l );
    test_positive_normal_( LDBL_MIN );
    test_negative_normal_( -LDBL_MIN );
    test_positive_normal_( LDBL_MAX );
    test_negative_normal_( -LDBL_MAX );
    test_positive_zero_( +0.0l );
    test_negative_zero_( -0.0l );
    test_positive_infinity_( std::numeric_limits<long double>::infinity() );
    test_negative_infinity_( -std::numeric_limits<long double>::infinity() );
    test_positive_nan_( std::numeric_limits<long double>::quiet_NaN() );
    test_negative_nan_( boost::core::copysign( std::numeric_limits<long double>::quiet_NaN(), -1.0l ) );
    test_positive_subnormal_( LDBL_MIN / 2 );
    test_negative_subnormal_( -LDBL_MIN / 2 );

    return boost::report_errors();
}
