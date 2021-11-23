//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_TEST_SUBIMAGE_TEST_HPP
#define BOOST_GIL_IO_TEST_SUBIMAGE_TEST_HPP

#include <boost/gil.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>

template <typename Image, typename Format>
void run_subimage_test(
    std::string const& filename, boost::gil::point_t const& top_left, boost::gil::point_t const& dimension)
{
    Image original, subimage;
    boost::gil::read_image(filename, original, Format{});
    boost::gil::image_read_settings<Format> settings(top_left, dimension);
    boost::gil::read_image(filename, subimage, settings);
    BOOST_TEST(boost::gil::equal_pixels(
        boost::gil::const_view(subimage),
        boost::gil::subimage_view(boost::gil::const_view(original), top_left, dimension)));
}

#endif // BOOST_GIL_IO_TEST_SUBIMAGE_TEST_HPP
