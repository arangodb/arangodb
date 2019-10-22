//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_TEST_COLOR_SPACE_WRITE_TEST_HPP
#define BOOST_GIL_IO_TEST_COLOR_SPACE_WRITE_TEST_HPP

#include <boost/gil.hpp>

#ifndef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
#include <boost/core/ignore_unused.hpp>
#endif
#include <string>

#include "cmp_view.hpp"

template< typename Tag >
void color_space_write_test( const std::string& file_name_1
                           , const std::string& file_name_2
                           )
{
    using namespace boost::gil;

    rgb8_image_t rgb( 320, 200 );
    bgr8_image_t bgr( 320, 200 );

    fill_pixels( view(rgb), rgb8_pixel_t(  0, 0, 255 ));
    fill_pixels( view(bgr), bgr8_pixel_t(255, 0,   0 ));

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( file_name_1, view( rgb ), Tag() );
    write_view( file_name_2, view( bgr ), Tag() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    rgb8_image_t rgb_1;
    rgb8_image_t rgb_2;

    read_image( file_name_1, rgb_1, Tag() );
    read_image( file_name_2, rgb_2, Tag() );

    cmp_view( view( rgb_1 ), view( rgb_2 ));
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifndef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    boost::ignore_unused(file_name_1);
    boost::ignore_unused(file_name_2);
#endif
}

#endif
