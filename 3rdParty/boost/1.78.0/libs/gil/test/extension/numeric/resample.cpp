//
// Copyright 2013 Krzysztof Czainski
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cmath>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

// FIXME: Remove when https://github.com/boostorg/core/issues/38 happens
#define BOOST_GIL_TEST_IS_CLOSE(a, b, epsilon) BOOST_TEST_LT(std::fabs((a) - (b)), (epsilon))

template <class F, class I>
struct test_map_fn
{
    using point_t = gil::point<F>;
    using result_type = point_t;
    result_type operator()(gil::point<I> const &src) const
    {
        F x = static_cast<F>(src.x) - 0.5;
        F y = static_cast<F>(src.y) - 0.5;
        return {x, y};
    }
};

namespace boost { namespace gil {

// NOTE: I suggest this could be the default behavior:

template <typename T>
struct mapping_traits;

template <class F, class I>
struct mapping_traits<test_map_fn<F, I>>
{
    using result_type = typename test_map_fn<F, I>::result_type;
};

template <class F, class I>
inline point<F> transform(test_map_fn<F, I> const &mf, point<I> const &src)
{
    return mf(src);
}

}} // namespace boost::gil

void test_bilinear_sampler_test()
{
    // R G B
    // G W R
    // B R G
    gil::rgb8_image_t img(3, 3);
    gil::rgb8_view_t v = view(img);
    v(0, 0) = v(1, 2) = v(2, 1) = gil::rgb8_pixel_t(128, 0, 0);
    v(0, 1) = v(1, 0) = v(2, 2) = gil::rgb8_pixel_t(0, 128, 0);
    v(0, 2) = v(2, 0) = gil::rgb8_pixel_t(0, 0, 128);
    v(1, 1) = gil::rgb8_pixel_t(128, 128, 128);

    gil::rgb8_image_t dims(4, 4);
    gil::rgb8c_view_t dv = gil::const_view(dims);

    test_map_fn<double, gil::rgb8_image_t::coord_t> mf;

    gil::resample_pixels(gil::const_view(img), gil::view(dims), mf, gil::bilinear_sampler());

    BOOST_TEST_EQ(gil::rgb8_pixel_t(128, 0, 0), dv(0, 0));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 64, 0), dv(0, 1));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(0, 64, 64), dv(0, 2));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(0, 0, 128), dv(0, 3));

    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 64, 0), dv(1, 0));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 96, 32), dv(1, 1));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 64, 64), dv(1, 2));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 0, 64), dv(1, 3));

    BOOST_TEST_EQ(gil::rgb8_pixel_t(0, 64, 64), dv(2, 0));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 64, 64), dv(2, 1));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(96, 64, 32), dv(2, 2));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 64, 0), dv(2, 3));

    BOOST_TEST_EQ(gil::rgb8_pixel_t(0, 0, 128), dv(3, 0));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 0, 64), dv(3, 1));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(64, 64, 0), dv(3, 2));
    BOOST_TEST_EQ(gil::rgb8_pixel_t(0, 128, 0), dv(3, 3));
}

int main()
{
    test_bilinear_sampler_test();

    return ::boost::report_errors();
}
