//
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/cmyk.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/pixel.hpp>
#include <boost/gil/rgb.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include "test_fixture.hpp"
#include "core/channel/test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;
namespace mp11 = boost::mp11;

// Test color_convert from any pixel to CMYK pixel types

template <typename SrcPixel>
struct test_cmyk_from_gray
{
    template<typename DstPixel>
    void operator()(DstPixel const&)
    {
        using pixel_src_t = SrcPixel;
        using pixel_dst_t = DstPixel;
        fixture::channel_value<typename gil::channel_type<pixel_src_t>::type> f_src;
        fixture::channel_value<typename gil::channel_type<pixel_dst_t>::type> f_dst;

        // FIXME: Where does this calculation come from? Shouldn't gray be inverted?
        //        Currently, white becomes black and black becomes white.
        {
            pixel_src_t const src{f_src.min_v_};
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.min_v_);
        }
        {
            pixel_src_t const src{f_src.max_v_};
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.max_v_);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each
        <
            mp11::mp_list
            <
                gil::cmyk8_pixel_t,
                gil::cmyk8s_pixel_t,
                gil::cmyk16_pixel_t,
                gil::cmyk16s_pixel_t,
                gil::cmyk32_pixel_t,
                gil::cmyk32s_pixel_t,
                gil::cmyk32f_pixel_t
            >
        >(test_cmyk_from_gray<SrcPixel>{});
    }
};

struct test_cmyk_from_rgb
{
    template <typename SrcPixel, typename DstPixel>
    void operator()(mp11::mp_list<SrcPixel, DstPixel> const&) const
    {
        using pixel_src_t = SrcPixel;
        using pixel_dst_t = DstPixel;
        fixture::channel_value<typename gil::channel_type<pixel_src_t>::type> f_src;
        fixture::channel_value<typename gil::channel_type<pixel_dst_t>::type> f_dst;

        // black
        {
            pixel_src_t const src(f_src.min_v_, f_src.min_v_, f_src.min_v_);
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.max_v_);
        }
        // white
        {
            pixel_src_t const src(f_src.max_v_, f_src.max_v_, f_src.max_v_);
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.min_v_);
        }
        // red
        {
            pixel_src_t const src(f_src.max_v_, f_src.min_v_, f_src.min_v_);
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.min_v_);
        }
        // green
        {
            pixel_src_t const src(f_src.min_v_, f_src.max_v_, f_src.min_v_);
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.min_v_);
        }
        // blue
        {
            pixel_src_t const src(f_src.min_v_, f_src.min_v_, f_src.max_v_);
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.min_v_);
        }
        // yellow
        {
            pixel_src_t const src(f_src.max_v_, f_src.max_v_, f_src.min_v_);
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.min_v_);
        }
        // cyan
        {
            pixel_src_t const src(f_src.min_v_, f_src.max_v_, f_src.max_v_);
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.min_v_);
        }
        // magenta
        {
            pixel_src_t const src(f_src.max_v_, f_src.min_v_, f_src.max_v_);
            pixel_dst_t dst;
            gil::color_convert(src, dst);

            BOOST_TEST_EQ(gil::get_color(dst, gil::cyan_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::magenta_t{}), f_dst.max_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::yellow_t{}), f_dst.min_v_);
            BOOST_TEST_EQ(gil::get_color(dst, gil::black_t{}), f_dst.min_v_);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each
        <
            mp11::mp_product
            <
                mp11::mp_list,
                mp11::mp_list
                <
                    gil::rgb8_pixel_t,
                    gil::rgb8s_pixel_t,
                    gil::rgb16_pixel_t,
                    gil::rgb16s_pixel_t,
                    gil::rgb32_pixel_t,
                    gil::rgb32s_pixel_t,
                    gil::rgb32f_pixel_t
                >,
                mp11::mp_list
                <
                    gil::cmyk8_pixel_t,
                    gil::cmyk16_pixel_t,
                    gil::cmyk32_pixel_t,
                    gil::cmyk32f_pixel_t
                    // FIXME: Conversion not handle properly signed CMYK pixels as destination
                >
            >
        >(test_cmyk_from_rgb{});
    }
};

int main()
{
    test_cmyk_from_gray<gil::gray8_pixel_t>::run();
    test_cmyk_from_gray<gil::gray8s_pixel_t>::run();
    test_cmyk_from_gray<gil::gray16_pixel_t>::run();
    test_cmyk_from_gray<gil::gray16s_pixel_t>::run();
    test_cmyk_from_gray<gil::gray32_pixel_t>::run();
    test_cmyk_from_gray<gil::gray32s_pixel_t>::run();
    test_cmyk_from_gray<gil::gray32f_pixel_t>::run();

    test_cmyk_from_rgb::run();

    return ::boost::report_errors();
}
