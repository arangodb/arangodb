//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_TEST_MODULE all_formats_test
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/extension/io/bmp.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/pnm.hpp>
#include <boost/gil/extension/io/targa.hpp>
#include <boost/gil/extension/io/tiff.hpp>

#include <boost/test/unit_test.hpp>

#include "paths.hpp"

// Test will include all format's headers and load and write some images.
// This test is more of a compilation test.

using namespace std;
using namespace boost::gil;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE( gil_io_tests )

BOOST_AUTO_TEST_CASE( non_bit_aligned_image_test )
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
    {
        rgb8_image_t img;
        read_image( bmp_filename, img, bmp_tag() );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
	fs::create_directories(fs::path(bmp_out));
        write_view( bmp_out + "all_formats_test.bmp", view( img ), bmp_tag() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }

    {
        rgb8_image_t img;
        read_image( jpeg_filename, img, jpeg_tag() );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
	fs::create_directories(fs::path(jpeg_out));
        write_view( jpeg_out + "all_formats_test.jpg", view( img ), jpeg_tag() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }

    {
        rgba8_image_t img;
        read_image( png_filename, img, png_tag() );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
	fs::create_directories(fs::path(png_out));
        write_view( png_out + "all_formats_test.png", view( img ), png_tag() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }

    {
        rgb8_image_t img;
        read_image( pnm_filename, img, pnm_tag() );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
	fs::create_directories(fs::path(pnm_out));
        write_view( pnm_out + "all_formats_test.pnm", view( img ), pnm_tag() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }

    {
        rgb8_image_t img;
        read_image( targa_filename, img, targa_tag() );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
	fs::create_directories(fs::path(targa_out));
        write_view( targa_out + "all_formats_test.tga", view( img ), targa_tag() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }

    {
        rgba8_image_t img;
        read_image( tiff_filename, img, tiff_tag() );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
	fs::create_directories(fs::path(tiff_out));
        write_view( tiff_out + "all_formats_test.tif", view( img ), tiff_tag() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
}

BOOST_AUTO_TEST_SUITE_END()
