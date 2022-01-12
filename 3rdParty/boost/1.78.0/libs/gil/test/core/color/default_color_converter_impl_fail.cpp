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

#include <boost/core/ignore_unused.hpp>
#include <boost/mp11.hpp>

#include <type_traits>

namespace gil = boost::gil;

struct unknown_color_space {};

int main()
{
    {
        gil::default_color_converter_impl<unknown_color_space, gil::gray_t> c;
        boost::ignore_unused(c);
    }
    {
        gil::default_color_converter_impl<gil::rgb_t, unknown_color_space> c;
        boost::ignore_unused(c);
    }
}
