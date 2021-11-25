//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/pixel.hpp>
#include <boost/gil/concepts/pixel.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/mp11.hpp>

#include <type_traits>

namespace gil = boost::gil;
using namespace boost::mp11;

template <typename Pixel>
struct assert_compatible
{
    template <typename CompatiblePixel>
    void operator()(CompatiblePixel&&)
    {
        using result_t = typename gil::pixels_are_compatible<Pixel, CompatiblePixel>::type;
        static_assert(result_t::value, "pixels should be compatible");

        // TODO: Refine after MPL -> MP11 switch
        static_assert(
            std::is_same<result_t, std::true_type>::value,
            "pixels_are_compatible result type should be std::true_type");

        static_assert(
            !std::is_same<result_t, std::false_type>::value,
            "pixels_are_compatible result type should no be std::false_type");
    }
};

template <typename Pixel>
struct assert_not_compatible
{
    template <typename NotCompatiblePixel>
    void operator()(NotCompatiblePixel&&)
    {
        static_assert(
            !gil::pixels_are_compatible<Pixel, NotCompatiblePixel>::value,
            "pixels should not be compatible");
    }
};

template <typename Pixel, typename... CompatiblePixels>
void test_compatible()
{
    mp_for_each<CompatiblePixels...>(assert_compatible<Pixel>());
}

template <typename Pixel, typename... CompatiblePixels>
void test_not_compatible()
{
    mp_for_each<CompatiblePixels...>(assert_not_compatible<Pixel>());
}

int main()
{
    test_compatible<gil::gray8_pixel_t, mp_list<
        gil::gray8_pixel_t,
        gil::gray8c_pixel_t>>();
    test_compatible<gil::gray8s_pixel_t, mp_list<
        gil::gray8s_pixel_t,
        gil::gray8sc_pixel_t>>();
    test_not_compatible<gil::gray8_pixel_t, mp_list<
        gil::gray8s_pixel_t,
        gil::gray8sc_pixel_t>>();

    test_compatible<gil::gray16_pixel_t, mp_list<
        gil::gray16_pixel_t,
        gil::gray16c_pixel_t>>();
    test_compatible<gil::gray16s_pixel_t, mp_list<
        gil::gray16s_pixel_t,
        gil::gray16sc_pixel_t>>();
    test_not_compatible<gil::gray16_pixel_t, mp_list<
        gil::gray16s_pixel_t,
        gil::gray16sc_pixel_t>>();

    test_compatible<gil::rgb8_pixel_t, mp_list<
        gil::bgr8_pixel_t,
        gil::bgr8c_pixel_t,
        gil::rgb8_pixel_t,
        gil::rgb8c_pixel_t>>();
    test_compatible<gil::rgb8s_pixel_t, mp_list<
        gil::bgr8s_pixel_t,
        gil::bgr8sc_pixel_t,
        gil::rgb8s_pixel_t,
        gil::rgb8sc_pixel_t>>();
    test_not_compatible<gil::rgb8_pixel_t, mp_list<
        gil::argb8_pixel_t,
        gil::abgr8_pixel_t,
        gil::rgba8_pixel_t,
        gil::bgr8s_pixel_t,
        gil::bgr8sc_pixel_t,
        gil::rgb8s_pixel_t,
        gil::rgb8sc_pixel_t>>();

    test_compatible<gil::rgba8_pixel_t, mp_list<
        gil::abgr8_pixel_t,
        gil::argb8_pixel_t,
        gil::bgra8_pixel_t,
        gil::bgra8c_pixel_t,
        gil::rgba8_pixel_t,
        gil::rgba8c_pixel_t>>();
    test_not_compatible<gil::rgba8_pixel_t, mp_list<
        gil::rgb8_pixel_t,
        gil::rgb16_pixel_t,
        gil::rgba16_pixel_t,
        gil::cmyk8_pixel_t,
        gil::cmyk16_pixel_t>>();

    test_compatible<gil::cmyk8_pixel_t, mp_list<
        gil::cmyk8_pixel_t,
        gil::cmyk8c_pixel_t>>();
    test_compatible<gil::cmyk8s_pixel_t, mp_list<
        gil::cmyk8s_pixel_t,
        gil::cmyk8sc_pixel_t>>();
    test_not_compatible<gil::cmyk8_pixel_t, mp_list<
        gil::cmyk8s_pixel_t,
        gil::cmyk8sc_pixel_t>>();

    test_compatible<gil::cmyk32f_pixel_t, mp_list<
        gil::cmyk32f_pixel_t,
        gil::cmyk32fc_pixel_t>>();

}
