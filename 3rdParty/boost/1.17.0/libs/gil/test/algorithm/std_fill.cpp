//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
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
#include <boost/core/lightweight_test.hpp>
#include <boost/range/algorithm/fill_n.hpp>

#include <array>
#include <cstdint>

namespace gil = boost::gil;

template <typename ArrayPixel>
void test_array_as_range()
{
    static_assert(ArrayPixel().size() == 2, "two-element array expected");

    gil::image<ArrayPixel> img(1, 1);
    std::fill(gil::view(img).begin(), gil::view(img).end(), ArrayPixel{0, 1});
    BOOST_TEST(*gil::view(img).at(0,0) == (ArrayPixel{0, 1}));
}

int main()
{
    test_array_as_range<boost::array<int, 2>>();
    test_array_as_range<std::array<int, 2>>();

    return boost::report_errors();
}
