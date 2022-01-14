//
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifdef BOOST_GIL_USE_CONCEPT_CHECK
// FIXME: Range as pixel does not seem to fulfill pixel concepts due to no specializations required:
//        pixel.hpp(50) : error C2039 : 'type' : is not a member of 'boost::gil::color_space_type<P>
#undef BOOST_GIL_USE_CONCEPT_CHECK
#endif
#include <boost/gil/algorithm.hpp>
#include <boost/gil/image.hpp>
#include <boost/gil/image_view.hpp>

#include <boost/array.hpp>
#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include <array>
#include <cstdint>

namespace gil = boost::gil;

using array_pixel_types = ::boost::mp11::mp_list
<
    boost::array<int, 2>,
    std::array<int, 2>
>;

struct test_array_as_pixel
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        gil::image<pixel_t> img(1, 1);
        std::fill(gil::view(img).begin(), gil::view(img).end(), pixel_t{0, 1});
        auto a = *gil::view(img).at(0, 0);
        auto e = pixel_t{0, 1};
        BOOST_TEST(a == e);
    }
    static void run()
    {
        boost::mp11::mp_for_each<array_pixel_types>(test_array_as_pixel{});
    }
};

int main()
{
    test_array_as_pixel::run();

    return ::boost::report_errors();
}
