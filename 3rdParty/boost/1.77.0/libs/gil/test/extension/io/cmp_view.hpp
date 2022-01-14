//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_TEST_EXTENSION_IO_CMP_VIEW_HPP
#define BOOST_GIL_IO_TEST_EXTENSION_IO_CMP_VIEW_HPP

#include <boost/gil.hpp>

#include <stdexcept>

template <typename View1, typename View2>
void cmp_view(View1 const& v1, View2 const& v2)
{
    if (v1.dimensions() != v2.dimensions())
        throw std::runtime_error("Images are not equal.");

    typename View1::x_coord_t width  = v1.width();
    typename View1::y_coord_t height = v1.height();

    for (typename View1::y_coord_t y = 0; y < height; ++y)
    {
        typename View1::x_iterator const src_it = v1.row_begin(y);
        typename View2::x_iterator const dst_it = v2.row_begin(y);

        for (typename View1::x_coord_t x = 0; x < width; ++x)
        {
            if (*src_it != *dst_it)
                throw std::runtime_error("Images are not equal.");
        }
    }
}

#endif // BOOST_GIL_IO_TEST_EXTENSION_IO_CMP_VIEW_HPP
