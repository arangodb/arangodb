//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE jpeg_write_test_module
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>

#include <boost/test/unit_test.hpp>

#include "color_space_write_test.hpp"
#include "mandel_view.hpp"
#include "paths.hpp"

using namespace std;
using namespace boost::gil;

using tag_t = jpeg_tag;

BOOST_AUTO_TEST_SUITE( gil_io_jpeg_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
BOOST_AUTO_TEST_CASE( write_test )
{
    // test writing all supported image types

    {
        write_view( jpeg_out + "gray8_test.jpg"
                  , create_mandel_view( 200, 200
                                      , gray8_pixel_t( 0   )
                                      , gray8_pixel_t( 255 )
                                      )
                  , tag_t()
                  );
    }

    {
        write_view( jpeg_out + "rgb8_test.jpg"
                  , create_mandel_view( 200, 200
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        write_view( jpeg_out + "cmyk8_test.jpg"
                  , create_mandel_view( 200, 200
                                      , cmyk8_pixel_t( 0,   0, 255, 127 )
                                      , cmyk8_pixel_t( 0, 255,   0, 127 )
                                      )
                  , tag_t()
                  );
    }
}
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( dct_method_write_test )
{
    {
        using image_t = rgb8_image_t;
        image_t img;

        read_image( jpeg_filename
                  , img
                  , tag_t()
                  );

        image_write_info< jpeg_tag > info;
        info._dct_method = jpeg_dct_method::fast;

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( jpeg_out + "fast_dct_write_test.jpg"
                  , view( img )
                  , info
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

    }
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( rgb_color_space_write_test )
{
    color_space_write_test< jpeg_tag >( jpeg_out + "rgb_color_space_test.jpg"
                                      , jpeg_out + "bgr_color_space_test.jpg"
                                      );
}

BOOST_AUTO_TEST_SUITE_END()
