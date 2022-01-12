//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/cmyka.hpp>

#include <boost/core/lightweight_test.hpp>

namespace gil = boost::gil;

void test_cmyka_to_rgba()
{
    gil::cmyka8_pixel_t a(10, 20, 30, 40, 50);
    gil::rgba8_pixel_t b;
    gil::color_convert(a, b);

    // TODO: no rgba to cmyka conversion implemented
    //gil::cmyka8_pixel_t c;
    //gil::color_convert( b, c );
    //BOOST_ASSERT(gil::at_c<0>(a) == gil::at_c<0>(c));
}

int main()
{
    test_cmyka_to_rgba();

    return boost::report_errors();
}
