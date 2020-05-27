// Copyright 2013 Krzysztof Czainski
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/// \file numeric.cpp

/// \brief Unit test for Numeric extension

#include <boost/gil/image.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>

#include <boost/assert.hpp>

#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

using namespace boost;
using namespace gil;

template < class F, class I >
struct TestMapFn
{
    using point_t = point<F>;
    using result_type = point_t;
    result_type operator()(point<I> const& src) const
    {
        F x = static_cast<F>( src.x ) - 0.5;
        F y = static_cast<F>( src.y ) - 0.5;
        return { x, y };
    }
};

namespace boost { namespace gil {

// NOTE: I suggest this could be the default behavior:

template <typename T> struct mapping_traits;

template < class F, class I >
struct mapping_traits<TestMapFn<F, I>>
{
    using result_type = typename TestMapFn<F, I>::result_type;
};

template <class F, class I>
inline point<F> transform(TestMapFn<F, I> const& mf, point<I> const& src)
{
    return mf(src);
}

}} // boost::gil


BOOST_AUTO_TEST_SUITE(Numeric_Tests)

BOOST_AUTO_TEST_CASE( pixel_numeric_operations_plus )
{
    rgb8_pixel_t a( 10, 20, 30 );
    bgr8_pixel_t b( 30, 20, 10 );

    pixel_plus_t< rgb8_pixel_t
                , bgr8_pixel_t
                , rgb8_pixel_t
                > op;
    rgb8_pixel_t c = op( a, b );

    BOOST_ASSERT( get_color( c, red_t()   ) == 20 );
    BOOST_ASSERT( get_color( c, green_t() ) == 40 );
    BOOST_ASSERT( get_color( c, blue_t()  ) == 60 );

    pixel_plus_t< rgb8_pixel_t
                , bgr8_pixel_t
                , bgr8_pixel_t
                > op2;
    bgr8_pixel_t d = op2( a, b );

    BOOST_ASSERT( get_color( d, red_t()   ) == 20 );
    BOOST_ASSERT( get_color( d, green_t() ) == 40 );
    BOOST_ASSERT( get_color( d, blue_t()  ) == 60 );
}

BOOST_AUTO_TEST_CASE( pixel_numeric_operations_multiply )
{
    rgb32f_pixel_t a( 1.f, 2.f, 3.f );
    bgr32f_pixel_t b( 2.f, 2.f, 2.f );

    pixel_multiply_t< rgb32f_pixel_t
                    , bgr32f_pixel_t
                    , rgb32f_pixel_t
                    > op;
    rgb32f_pixel_t c = op( a, b );

    float epsilon = 1e-6f;
    BOOST_CHECK_CLOSE( static_cast<float>( get_color( c,   red_t() )), 2.f, epsilon );
    BOOST_CHECK_CLOSE( static_cast<float>( get_color( c, green_t() )), 4.f, epsilon );
    BOOST_CHECK_CLOSE( static_cast<float>( get_color( c,  blue_t() )), 6.f, epsilon );
}

BOOST_AUTO_TEST_CASE( pixel_numeric_operations_divide )
{
    // integer
    {
        rgb8_pixel_t a( 10, 20, 30 );
        bgr8_pixel_t b(  2,  2,  2 );

        pixel_divide_t< rgb8_pixel_t
                      , bgr8_pixel_t
                      , rgb8_pixel_t
                      > op;
        rgb32f_pixel_t c = op( a, b );

        BOOST_ASSERT( get_color( c,   red_t() ) ==  5 );
        BOOST_ASSERT( get_color( c, green_t() ) == 10 );
        BOOST_ASSERT( get_color( c,  blue_t() ) == 15 );
    }

    // float
    {
        rgb32f_pixel_t a( 1.f, 2.f, 3.f );
        bgr32f_pixel_t b( 2.f, 2.f, 2.f );

        pixel_divide_t< rgb32f_pixel_t
                      , bgr32f_pixel_t
                      , rgb32f_pixel_t
                      > op;
        rgb32f_pixel_t c = op( a, b );

        float epsilon = 1e-6f;
        BOOST_CHECK_CLOSE( static_cast< float >( get_color( c,   red_t() )), 0.5f, epsilon );
        BOOST_CHECK_CLOSE( static_cast< float >( get_color( c, green_t() )),  1.f, epsilon );
        BOOST_CHECK_CLOSE( static_cast< float >( get_color( c,  blue_t() )), 1.5f, epsilon );
    }
}

BOOST_AUTO_TEST_CASE(bilinear_sampler_test)
{
    // R G B
    // G W R
    // B R G
    rgb8_image_t img(3,3);
    rgb8_view_t v = view(img);
    v(0,0) = v(1,2) = v(2,1) = rgb8_pixel_t(128,0,0);
    v(0,1) = v(1,0) = v(2,2) = rgb8_pixel_t(0,128,0);
    v(0,2) = v(2,0) = rgb8_pixel_t(0,0,128);
    v(1,1) = rgb8_pixel_t(128,128,128);

    rgb8_image_t dimg(4,4);
    rgb8c_view_t dv = const_view(dimg);

    TestMapFn<double,rgb8_image_t::coord_t> mf;

    resample_pixels(const_view(img), view(dimg), mf, bilinear_sampler());

    BOOST_ASSERT(rgb8_pixel_t(128,0,0) == dv(0,0));
    BOOST_ASSERT(rgb8_pixel_t(64,64,0) == dv(0,1));
    BOOST_ASSERT(rgb8_pixel_t(0,64,64) == dv(0,2));
    BOOST_ASSERT(rgb8_pixel_t(0,0,128) == dv(0,3));

    BOOST_ASSERT(rgb8_pixel_t(64,64,0) == dv(1,0));
    BOOST_ASSERT(rgb8_pixel_t(64,96,32) == dv(1,1));
    BOOST_ASSERT(rgb8_pixel_t(64,64,64) == dv(1,2));
    BOOST_ASSERT(rgb8_pixel_t(64,0,64) == dv(1,3));

    BOOST_ASSERT(rgb8_pixel_t(0,64,64) == dv(2,0));
    BOOST_ASSERT(rgb8_pixel_t(64,64,64) == dv(2,1));
    BOOST_ASSERT(rgb8_pixel_t(96,64,32) == dv(2,2));
    BOOST_ASSERT(rgb8_pixel_t(64,64,0) == dv(2,3));

    BOOST_ASSERT(rgb8_pixel_t(0,0,128) == dv(3,0));
    BOOST_ASSERT(rgb8_pixel_t(64,0,64) == dv(3,1));
    BOOST_ASSERT(rgb8_pixel_t(64,64,0) == dv(3,2));
    BOOST_ASSERT(rgb8_pixel_t(0,128,0) == dv(3,3));
}

BOOST_AUTO_TEST_SUITE_END()
