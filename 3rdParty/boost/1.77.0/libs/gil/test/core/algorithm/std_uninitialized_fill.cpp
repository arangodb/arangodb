//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include <memory>
#include <random>
#include <type_traits>
#include <vector>

#include "test_utility_output_stream.hpp"
#include "core/channel/test_fixture.hpp"
#include "core/image/test_fixture.hpp"
#include "core/pixel/test_fixture.hpp"

namespace boost { namespace gil { namespace test { namespace fixture {

template <typename Pixel>
struct pixel_array
{
    using iterator = Pixel*;
#ifdef NDEBUG
    constexpr static std::size_t default_x_size = 256;
    constexpr static std::size_t default_y_size = 128;
#else
    constexpr static std::size_t default_x_size = 16;
    constexpr static std::size_t default_y_size = 8;
#endif

    pixel_array(std::size_t x_size = default_x_size, std::size_t y_size = default_y_size)
        : pixels_(new Pixel[x_size * y_size])
        , x_size_(x_size)
        , y_size_(y_size)
    {}

    auto begin() -> iterator { return pixels_.get(); }
    auto end() -> iterator { return pixels_.get() + x_size_ * y_size_; }

private:
    std::unique_ptr<Pixel[]> pixels_;
    std::size_t x_size_;
    std::size_t y_size_;
};

}}}}

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

struct fill_with_pixel_integer_types
{
    template <typename Pixel>
    void operator()(Pixel const &)
    {
        using pixel_t = Pixel;
        auto min_pixel = fixture::pixel_generator<pixel_t>::min();
        auto max_pixel = fixture::pixel_generator<pixel_t>::max();
        auto rnd_pixel = fixture::pixel_generator<pixel_t>::random();

        for (auto const &fill_pixel : {min_pixel, max_pixel, rnd_pixel})
        {
            fixture::pixel_array<pixel_t> pixels;
            std::uninitialized_fill(pixels.begin(), pixels.end(), fill_pixel);

            for (pixel_t const &p : pixels)
                BOOST_TEST_EQ(p, fill_pixel);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(fill_with_pixel_integer_types{});
    }
};

struct fill_with_pixel_float_types
{
    template <typename Pixel>
    void operator()(Pixel const &)
    {
        using pixel_t = Pixel;
        auto min_pixel = fixture::pixel_generator<pixel_t>::min();
        auto max_pixel = fixture::pixel_generator<pixel_t>::max();

        for (auto const &fill_pixel : {min_pixel, max_pixel})
        {
            fixture::pixel_array<Pixel> pixels;
            std::uninitialized_fill(pixels.begin(), pixels.end(), fill_pixel);

            for (Pixel const &p : pixels)
                BOOST_TEST_EQ(p, fill_pixel);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_float_types>(fill_with_pixel_float_types{});
    }
};

void
test_fill_with_packed_pixel_gray3()
{
    auto min_pixel = fixture::packed_pixel_gray3{0};
    auto mid_pixel = fixture::packed_pixel_gray3{3};
    auto max_pixel = fixture::packed_pixel_gray3{7};

    for (auto const& fill_pixel : {min_pixel, max_pixel, mid_pixel} )
    {
        fixture::pixel_array<fixture::packed_pixel_gray3> pixels;
        std::uninitialized_fill(pixels.begin(), pixels.end(), fill_pixel);

        for (fixture::packed_pixel_gray3 const& p : pixels)
        {
            BOOST_TEST_EQ(p, fill_pixel);
            BOOST_TEST_EQ((int)get_color(p, gil::gray_color_t()), (int)get_color(fill_pixel, gil::gray_color_t()));
        }
    }
}

void test_fill_with_packed_pixel_bgr121()
{
    auto min_pixel = fixture::packed_pixel_bgr121{0};
    auto mid_pixel = fixture::packed_pixel_bgr121{8};
    auto max_pixel = fixture::packed_pixel_bgr121{17};

    for (auto const& fill_pixel : {min_pixel, max_pixel, mid_pixel} )
    {
        fixture::pixel_array<fixture::packed_pixel_bgr121> pixels;
        std::uninitialized_fill(pixels.begin(), pixels.end(), fill_pixel);

        for (fixture::packed_pixel_bgr121 const& p : pixels)
        {
            BOOST_TEST_EQ(p, fill_pixel);
            BOOST_TEST_EQ((int)get_color(p, gil::red_t()), (int)get_color(fill_pixel, gil::red_t()));
            BOOST_TEST_EQ((int)get_color(p, gil::green_t()), (int)get_color(fill_pixel, gil::green_t()));
            BOOST_TEST_EQ((int)get_color(p, gil::blue_t()), (int)get_color(fill_pixel, gil::blue_t()));
        }
    }
}

void test_fill_with_packed_pixel_rgb535()
{
    fixture::packed_pixel_rgb535 min_pixel(0, 0, 0);
    fixture::packed_pixel_rgb535 mid_pixel(15, 3, 15);
    fixture::packed_pixel_rgb535 max_pixel(31, 7, 31);

    for (auto const& fill_pixel : {min_pixel, max_pixel, mid_pixel} )
    {
        fixture::pixel_array<fixture::packed_pixel_rgb535> pixels;
        std::uninitialized_fill(pixels.begin(), pixels.end(), fill_pixel);

        for (fixture::packed_pixel_rgb535 const& p : pixels)
        {
            BOOST_TEST_EQ(p, fill_pixel);
            BOOST_TEST_EQ((int)get_color(p, gil::red_t()), (int)get_color(fill_pixel, gil::red_t()));
            BOOST_TEST_EQ((int)get_color(p, gil::green_t()), (int)get_color(fill_pixel, gil::green_t()));
            BOOST_TEST_EQ((int)get_color(p, gil::blue_t()), (int)get_color(fill_pixel, gil::blue_t()));
        }
    }
}

void test_bit_aligned_pixel_bgr232()
{
    fixture::bit_aligned_pixel_bgr232 min_pixel(0, 0, 0);
    fixture::bit_aligned_pixel_bgr232 mid_pixel(1, 4, 2);
    fixture::bit_aligned_pixel_bgr232 max_pixel(3, 7, 3);

    for (auto const& fill_pixel : {min_pixel, max_pixel, mid_pixel} )
    {
        fixture::pixel_array<fixture::bit_aligned_pixel_bgr232> pixels;
        std::uninitialized_fill(pixels.begin(), pixels.end(), fill_pixel);

        for (fixture::bit_aligned_pixel_bgr232 const& p : pixels)
        {
            BOOST_TEST_EQ(p, fill_pixel);
            BOOST_TEST_EQ((int)get_color(p, gil::red_t()), (int)get_color(fill_pixel, gil::red_t()));
            BOOST_TEST_EQ((int)get_color(p, gil::green_t()), (int)get_color(fill_pixel, gil::green_t()));
            BOOST_TEST_EQ((int)get_color(p, gil::blue_t()), (int)get_color(fill_pixel, gil::blue_t()));
        }
    }
}

void test_bit_aligned_pixel_rgb567()
{
    fixture::bit_aligned_pixel_rgb567 min_pixel(0, 0, 0);
    fixture::bit_aligned_pixel_rgb567 mid_pixel(15, 31, 63);
    fixture::bit_aligned_pixel_rgb567 max_pixel(31, 63, 127);

    for (auto const& fill_pixel : {min_pixel, max_pixel, mid_pixel} )
    {
        fixture::pixel_array<fixture::bit_aligned_pixel_rgb567> pixels;
        std::uninitialized_fill(pixels.begin(), pixels.end(), fill_pixel);

        for (fixture::bit_aligned_pixel_rgb567 const& p : pixels)
        {
            BOOST_TEST_EQ(p, fill_pixel);
            BOOST_TEST_EQ((int)get_color(p, gil::red_t()), (int)get_color(fill_pixel, gil::red_t()));
            BOOST_TEST_EQ((int)get_color(p, gil::green_t()), (int)get_color(fill_pixel, gil::green_t()));
            BOOST_TEST_EQ((int)get_color(p, gil::blue_t()), (int)get_color(fill_pixel, gil::blue_t()));
        }
    }
}

int main()
{
    fill_with_pixel_integer_types::run();
    fill_with_pixel_float_types::run();

    test_fill_with_packed_pixel_gray3();
    test_fill_with_packed_pixel_bgr121();
    test_fill_with_packed_pixel_rgb535();

    test_bit_aligned_pixel_bgr232();
    test_bit_aligned_pixel_rgb567();

    return ::boost::report_errors();
}
