//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE targa_read_test_module

#include <boost/gil.hpp>
#include <boost/gil/extension/io/targa.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/type_traits/is_same.hpp>

#include "paths.hpp"
#include "scanline_read_test.hpp"

using namespace std;
using namespace boost::gil;

using tag_t = targa_tag;

BOOST_AUTO_TEST_SUITE( gil_io_targa_tests )

#ifdef BOOST_GIL_IO_USE_TARGA_FILEFORMAT_TEST_SUITE_IMAGES

template< typename Image >
void test_targa_scanline_reader( string filename )
{
    test_scanline_reader<Image, targa_tag>( string( targa_in + filename ).c_str() );
}

template< typename Image >
void write( Image&        img
          , const string& file_name
          )
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( targa_out + file_name
              , view( img )
              , tag_t()
              );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( read_header_test )
{
    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( targa_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._header_size     , 18  );
        BOOST_CHECK_EQUAL( backend._info._offset          , 18  );
        BOOST_CHECK_EQUAL( backend._info._color_map_type  , 0   );
        BOOST_CHECK_EQUAL( backend._info._image_type      , 10  );
        BOOST_CHECK_EQUAL( backend._info._color_map_start , 0   );
        BOOST_CHECK_EQUAL( backend._info._color_map_length, 0   );
        BOOST_CHECK_EQUAL( backend._info._color_map_depth , 0   );
        BOOST_CHECK_EQUAL( backend._info._x_origin        , 0   );
        BOOST_CHECK_EQUAL( backend._info._y_origin        , 0   );
        BOOST_CHECK_EQUAL( backend._info._width           , 124 );
        BOOST_CHECK_EQUAL( backend._info._height          , 124 );
        BOOST_CHECK_EQUAL( backend._info._bits_per_pixel  , 24  );
        BOOST_CHECK_EQUAL( backend._info._descriptor      , 0   );
    }
}

BOOST_AUTO_TEST_CASE( read_reference_images_test )
{
    // 24BPP_compressed.tga
    {
        rgb8_image_t img;
        read_image( targa_in + "24BPP_compressed.tga", img, tag_t() );

        typename rgb8_image_t::x_coord_t width  = view( img ).width();
        typename rgb8_image_t::y_coord_t height = view( img ).height();

        BOOST_CHECK_EQUAL( width , 124 );
        BOOST_CHECK_EQUAL( height, 124 );
        BOOST_CHECK( view( img )(0, 0) == rgb8_pixel_t(248, 0, 248) );
        BOOST_CHECK( view( img )(width-1, 0) == rgb8_pixel_t(0, 0, 248) );
        BOOST_CHECK( view( img )(0, height-1) == rgb8_pixel_t(248, 0, 0) );
        BOOST_CHECK( view( img )(width-1, height-1) == rgb8_pixel_t(248, 0, 248) );

        write( img, "24BPP_compressed_out.tga" );
    }

    // 24BPP_uncompressed.tga
    {
        rgb8_image_t img;
        read_image( targa_in + "24BPP_uncompressed.tga", img, tag_t() );

        typename rgb8_image_t::x_coord_t width  = view( img ).width();
        typename rgb8_image_t::y_coord_t height = view( img ).height();

        BOOST_CHECK_EQUAL( width , 124 );
        BOOST_CHECK_EQUAL( height, 124 );
        BOOST_CHECK( view( img )(0, 0) == rgb8_pixel_t(248, 0, 248) );
        BOOST_CHECK( view( img )(width-1, 0) == rgb8_pixel_t(0, 0, 248) );
        BOOST_CHECK( view( img )(0, height-1) == rgb8_pixel_t(248, 0, 0) );
        BOOST_CHECK( view( img )(width-1, height-1) == rgb8_pixel_t(248, 0, 248) );

        write( img, "24BPP_uncompressed_out.tga" );

        test_targa_scanline_reader< bgr8_image_t >( "24BPP_uncompressed.tga" );
    }

    // 32BPP_compressed.tga
    {
        rgba8_image_t img;
        read_image( targa_in + "32BPP_compressed.tga", img, tag_t() );

        typename rgba8_image_t::x_coord_t width  = view( img ).width();
        typename rgba8_image_t::y_coord_t height = view( img ).height();

        BOOST_CHECK_EQUAL( width , 124 );
        BOOST_CHECK_EQUAL( height, 124 );
        BOOST_CHECK( view( img )(0, 0) == rgba8_pixel_t(248, 0, 248, 255) );
        BOOST_CHECK( view( img )(width-1, 0) == rgba8_pixel_t(0, 0, 248, 255) );
        BOOST_CHECK( view( img )(0, height-1) == rgba8_pixel_t(0, 0, 0, 0) );
        BOOST_CHECK( view( img )(width-1, height-1) == rgba8_pixel_t(248, 0, 248, 255) );

        write( img, "32BPP_compressed_out.tga" );
    }

    // 32BPP_uncompressed.tga
    {
        rgba8_image_t img;
        read_image( targa_in + "32BPP_uncompressed.tga", img, tag_t() );

        typename rgba8_image_t::x_coord_t width  = view( img ).width();
        typename rgba8_image_t::y_coord_t height = view( img ).height();

        BOOST_CHECK_EQUAL( width , 124 );
        BOOST_CHECK_EQUAL( height, 124 );
        BOOST_CHECK( view( img )(0, 0) == rgba8_pixel_t(248, 0, 248, 255) );
        BOOST_CHECK( view( img )(width-1, 0) == rgba8_pixel_t(0, 0, 248, 255) );
        BOOST_CHECK( view( img )(0, height-1) == rgba8_pixel_t(0, 0, 0, 0) );
        BOOST_CHECK( view( img )(width-1, height-1) == rgba8_pixel_t(248, 0, 248, 255) );

        write( img, "32BPP_uncompressed_out.tga" );

        test_targa_scanline_reader< bgra8_image_t >( "32BPP_uncompressed.tga" );
    }

    // 24BPP_compressed_ul_origin.tga
    {
        rgb8_image_t img;
        read_image( targa_in + "24BPP_compressed_ul_origin.tga", img, tag_t() );

        typename rgb8_image_t::x_coord_t width  = view( img ).width();
        typename rgb8_image_t::y_coord_t height = view( img ).height();

        BOOST_CHECK_EQUAL( width , 124 );
        BOOST_CHECK_EQUAL( height, 124 );
        BOOST_CHECK( view( img )(0, 0) == rgb8_pixel_t(248, 0, 248) );
        BOOST_CHECK( view( img )(width-1, 0) == rgb8_pixel_t(0, 0, 248) );
        BOOST_CHECK( view( img )(0, height-1) == rgb8_pixel_t(248, 0, 0) );
        BOOST_CHECK( view( img )(width-1, height-1) == rgb8_pixel_t(248, 0, 248) );

        write( img, "24BPP_compressed_ul_origin_out.tga" );
    }

    // 24BPP_uncompressed_ul_origin.tga
    {
        rgb8_image_t img;
        read_image( targa_in + "24BPP_uncompressed_ul_origin.tga", img, tag_t() );

        typename rgb8_image_t::x_coord_t width  = view( img ).width();
        typename rgb8_image_t::y_coord_t height = view( img ).height();

        BOOST_CHECK_EQUAL( width , 124 );
        BOOST_CHECK_EQUAL( height, 124 );
        BOOST_CHECK( view( img )(0, 0) == rgb8_pixel_t(248, 0, 248) );
        BOOST_CHECK( view( img )(width-1, 0) == rgb8_pixel_t(0, 0, 248) );
        BOOST_CHECK( view( img )(0, height-1) == rgb8_pixel_t(248, 0, 0) );
        BOOST_CHECK( view( img )(width-1, height-1) == rgb8_pixel_t(248, 0, 248) );

        write( img, "24BPP_uncompressed_ul_origin_out.tga" );
    }

    // 32BPP_compressed_ul_origin.tga
    {
        rgba8_image_t img;
        read_image( targa_in + "32BPP_compressed_ul_origin.tga", img, tag_t() );

        typename rgba8_image_t::x_coord_t width  = view( img ).width();
        typename rgba8_image_t::y_coord_t height = view( img ).height();

        BOOST_CHECK_EQUAL( width , 124 );
        BOOST_CHECK_EQUAL( height, 124 );
        BOOST_CHECK( view( img )(0, 0) == rgba8_pixel_t(248, 0, 248, 255) );
        BOOST_CHECK( view( img )(width-1, 0) == rgba8_pixel_t(0, 0, 248, 255) );
        BOOST_CHECK( view( img )(0, height-1) == rgba8_pixel_t(0, 0, 0, 0) );
        BOOST_CHECK( view( img )(width-1, height-1) == rgba8_pixel_t(248, 0, 248, 255) );

        write( img, "32BPP_compressed_ul_origin_out.tga" );
    }

    // 32BPP_uncompressed_ul_origin.tga
    {
        rgba8_image_t img;
        read_image( targa_in + "32BPP_uncompressed_ul_origin.tga", img, tag_t() );

        typename rgba8_image_t::x_coord_t width  = view( img ).width();
        typename rgba8_image_t::y_coord_t height = view( img ).height();

        BOOST_CHECK_EQUAL( width , 124 );
        BOOST_CHECK_EQUAL( height, 124 );
        BOOST_CHECK( view( img )(0, 0) == rgba8_pixel_t(248, 0, 248, 255) );
        BOOST_CHECK( view( img )(width-1, 0) == rgba8_pixel_t(0, 0, 248, 255) );
        BOOST_CHECK( view( img )(0, height-1) == rgba8_pixel_t(0, 0, 0, 0) );
        BOOST_CHECK( view( img )(width-1, height-1) == rgba8_pixel_t(248, 0, 248, 255) );

        write( img, "32BPP_uncompressed_ul_origin_out.tga" );
    }
}

BOOST_AUTO_TEST_CASE( partial_image_test )
{
    const std::string filename( targa_in + "24BPP_compressed.tga" );

    {
        rgb8_image_t img;
        read_image( filename
                  , img
                  , image_read_settings< targa_tag >( point_t( 0, 0 ), point_t( 50, 50 ) )
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( targa_out + "targa_partial.tga"
                  , view( img )
                  , tag_t()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

#endif // BOOST_GIL_IO_USE_TARGA_FILEFORMAT_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
