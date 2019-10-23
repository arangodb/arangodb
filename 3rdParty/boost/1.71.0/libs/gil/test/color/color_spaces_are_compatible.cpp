//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/color_base.hpp>
#include <boost/gil/concepts.hpp>
#include <boost/gil/device_n.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/rgb.hpp>
#include <boost/gil/rgba.hpp>
#include <boost/gil/cmyk.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/mp11.hpp>

#include <type_traits>

namespace gil = boost::gil;
using namespace boost::mp11;

template <typename T>
using test_self_compatible = gil::color_spaces_are_compatible<T, T>;

int main()
{
    using color_spaces = mp_list
    <
        gil::devicen_t<2>,
        gil::devicen_t<3>,
        gil::devicen_t<4>,
        gil::devicen_t<5>,
        gil::gray_t,
        gil::cmyk_t,
        gil::rgb_t,
        gil::rgba_t
    >;

    static_assert(std::is_same
        <
            mp_all_of<color_spaces, test_self_compatible>,
            std::true_type
        >::value,
        "color_spaces_are_compatible should yield true for the same types");
}
