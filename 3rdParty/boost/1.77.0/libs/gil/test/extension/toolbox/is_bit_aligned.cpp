//
// Copyright 2013 Christian Henning
// Copyright 2020 Mateusz Loskot <mateusz@loskot.net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/metafunctions/is_bit_aligned.hpp>

namespace gil = boost::gil;

void test_is_bit_aligned()
{
    using image_t = gil::bit_aligned_image1_type< 4, gil::gray_layout_t>::type;
    static_assert(gil::is_bit_aligned<image_t::view_t::value_type>::value, "");
}

int main()
{
    test_is_bit_aligned();
}

