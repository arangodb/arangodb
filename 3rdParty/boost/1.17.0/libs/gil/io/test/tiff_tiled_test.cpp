//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE tiff_tiled_test_module
#include <boost/test/unit_test.hpp>

#include <boost/gil/extension/io/tiff.hpp>
#include "paths.hpp"

using namespace std;
using namespace boost;
using namespace gil;

using tag_t = tiff_tag;

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_CASE( read_tile_infos_test )
{
    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( tiff_in_GM + "tiger-minisblack-float-tile-16.tif"
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._tile_width , 16 );
        BOOST_CHECK_EQUAL( backend._info._tile_length, 16 );
    }

    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( tiff_in_GM + "tiger-minisblack-tile-08.tif"
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._tile_width , 16 );
        BOOST_CHECK_EQUAL( backend._info._tile_length, 16 );
    }

    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( tiff_in_GM + "tiger-palette-tile-08.tif"
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._tile_width , 16 );
        BOOST_CHECK_EQUAL( backend._info._tile_length, 16 );
    }

    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( tiff_in_GM + "tiger-rgb-tile-contig-08.tif"
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._tile_width , 16 );
        BOOST_CHECK_EQUAL( backend._info._tile_length, 16 );
    }

    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( tiff_in_GM + "tiger-rgb-tile-planar-08.tif"
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._tile_width , 16 );
        BOOST_CHECK_EQUAL( backend._info._tile_length, 16 );
    }
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
