//
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/color_convert.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/rgb.hpp>
#include <boost/gil/rgba.hpp>
#include <boost/gil/cmyk.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include <type_traits>

#include "test_utility_output_stream.hpp"
#include "core/pixel/test_fixture.hpp"

namespace gil = boost::gil;
namespace mp11 = boost::mp11;

#ifdef BOOST_GIL_TEST_DEBUG
#include <boost/core/demangle.hpp>
#include <iostream>
namespace {
template <class T>
std::string name() { return boost::core::demangle(typeid(T).name()); }
}
#endif

template <typename ColorSpaces>
struct test_roundtrip_convertible
{
    template<typename Src, typename Dst>
    void operator()(mp11::mp_list<Src, Dst> const&) const
    {
#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "test_lossless_roundtrip:\n"
                  << "\tsrc: " << name<Src>() << "\n\tdst: " << name<Dst>() << std::endl;
#endif
        using pixel_src_t = gil::pixel<std::uint8_t, gil::layout<Src>>;
        using pixel_dst_t = gil::pixel<std::uint8_t, gil::layout<Dst>>;

        pixel_src_t src1{};
        pixel_dst_t dst1{};

        gil::default_color_converter_impl<Src, Dst> convert_to;
        convert_to(src1, dst1);

        gil::default_color_converter_impl<Dst, Src> convert_from;
        pixel_src_t src2{};
        convert_from(dst1, src2);

        BOOST_TEST_EQ(src1, src2);
    }
    static void run()
    {
        boost::mp11::mp_for_each
        <
            mp11::mp_product<mp11::mp_list, ColorSpaces, ColorSpaces>
        >(test_roundtrip_convertible{});
    }
};

template <typename ColorSpaces>
struct test_convertible
{
    template<typename Src, typename Dst>
    void operator()(mp11::mp_list<Src, Dst> const&) const
    {
#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "test_all:\n"
                  << "\tsrc: " << name<Src>() << "\n\tdst: " << name<Dst>() << std::endl;
#endif
        using pixel_src_t = gil::pixel<std::uint8_t, gil::layout<Src>>;
        using pixel_dst_t = gil::pixel<std::uint8_t, gil::layout<Dst>>;

        pixel_src_t src{};
        pixel_dst_t dst{};

        gil::default_color_converter_impl<Src, Dst> convert_to;
        convert_to(src, dst);
    }
    static void run()
    {
        boost::mp11::mp_for_each
        <
            mp11::mp_product<mp11::mp_list, ColorSpaces, ColorSpaces>
        >(test_convertible{});
    }
};

int main()
{
    test_convertible
    <
        mp11::mp_list<gil::cmyk_t, gil::gray_t, gil::rgb_t, gil::rgba_t>
    >::run();

    test_roundtrip_convertible
    <
        mp11::mp_list<gil::gray_t, gil::rgb_t>
    >::run();

    test_roundtrip_convertible<mp11::mp_list<gil::cmyk_t>>::run();
    test_roundtrip_convertible<mp11::mp_list<gil::gray_t>>::run();
    test_roundtrip_convertible<mp11::mp_list<gil::rgba_t>>::run();
    return ::boost::report_errors();
}
