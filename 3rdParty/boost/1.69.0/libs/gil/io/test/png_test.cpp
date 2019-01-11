//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_TEST_MODULE png_test
//#define BOOST_GIL_IO_PNG_FLOATING_POINT_SUPPORTED
//#define BOOST_GIL_IO_PNG_FIXED_POINT_SUPPORTED

#include <boost/gil.hpp>
#include <boost/gil/extension/io/png.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/type_traits/is_same.hpp>

#include <fstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

using namespace std;
using namespace boost;
using namespace gil;

typedef png_tag tag_t;

BOOST_AUTO_TEST_SUITE( gil_io_png_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( read_image_info_using_string )
{
    {
        typedef get_reader_backend< const std::string
                                  , tag_t
                                  >::type backend_t;

        backend_t backend = read_image_info( png_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }

    {
        ifstream in( png_filename.c_str(), ios::binary );

        typedef get_reader_backend< ifstream
                                  , tag_t
                                  >::type backend_t;

        backend_t backend = read_image_info( in
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }

    {
        FILE* file = fopen( png_filename.c_str(), "rb" );

        typedef get_reader_backend< FILE*
                                  , tag_t
                                  >::type backend_t;

        backend_t backend = read_image_info( file
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }
}

BOOST_AUTO_TEST_CASE( read_image_test )
{
    {
        rgba8_image_t img;
        read_image( png_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        ifstream in( png_filename.c_str(), ios::binary );

        rgba8_image_t img;
        read_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        FILE* file = fopen( png_filename.c_str(), "rb" );

        rgba8_image_t img;
        read_image( file, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }
}

BOOST_AUTO_TEST_CASE( read_and_convert_image_test )
{
    {
        rgb8_image_t img;
        read_and_convert_image( png_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        rgba8_image_t img;
        read_and_convert_image( png_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        ifstream in( png_filename.c_str(), ios::binary );

        rgb8_image_t img;
        read_and_convert_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        FILE* file = fopen( png_filename.c_str(), "rb" );

        rgb8_image_t img;
        read_and_convert_image( file, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }
}

BOOST_AUTO_TEST_CASE( read_view_test )
{
    {
        rgba8_image_t img( 1000, 600 );
        read_view( png_filename, view( img ), tag_t() );
    }

    {
        ifstream in( png_filename.c_str(), ios::binary );

        rgba8_image_t img( 1000, 600 );
        read_view( in, view( img ), tag_t() );
    }

    {
        FILE* file = fopen( png_filename.c_str(), "rb" );

        rgba8_image_t img( 1000, 600 );
        read_view( file, view( img ), tag_t() );
    }
}

BOOST_AUTO_TEST_CASE( read_and_convert_view_test )
{
    {
        rgb8_image_t img( 1000, 600 );
        read_and_convert_view( png_filename, view( img ), tag_t() );
    }

    {
        ifstream in( png_filename.c_str(), ios::binary );

        rgb8_image_t img( 1000, 600 );
        read_and_convert_view( in, view( img ), tag_t() );
    }

    {
        FILE* file = fopen( png_filename.c_str(), "rb" );

        rgb8_image_t img( 1000, 600 );

        read_and_convert_view( file
                             , view( img )
                             , tag_t()
                             );
    }
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
BOOST_AUTO_TEST_CASE( write_view_test )
{
    {
        string filename( png_out + "write_test_string.png" );

        write_view( filename
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( png_out + "write_test_string_bgr.png" );

        write_view( filename
                  , create_mandel_view( 320, 240
                                      , bgr8_pixel_t( 255, 0, 0 )
                                      , bgr8_pixel_t( 0, 255, 0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( png_out + "write_test_ofstream.png" );

        ofstream out( filename.c_str(), ios::out | ios::binary );

        write_view( out
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( png_out + "write_test_file.png" );

        FILE* file = fopen( filename.c_str(), "wb" );

        write_view( file
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( png_out + "write_test_info.png" );
        FILE* file = fopen( filename.c_str(), "wb" );

        image_write_info< png_tag > info;

        write_view( file
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , info
                  );
    }
}
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
BOOST_AUTO_TEST_CASE( stream_test )
{
    // 1. Read an image.
    ifstream in( png_filename.c_str(), ios::binary );

    rgba8_image_t img;
    read_image( in, img, tag_t() );

    // 2. Write image to in-memory buffer.
    stringstream out_buffer( ios_base::in | ios_base::out | ios_base::binary );
    write_view( out_buffer, view( img ), tag_t() );

    // 3. Copy in-memory buffer to another.
    stringstream in_buffer( ios_base::in | ios_base::out | ios_base::binary );
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    rgba8_image_t dst;
    read_image( in_buffer, dst, tag_t() );

    // 5. Write out image.
    string filename( png_out + "stream_test.png" );
    ofstream out( filename.c_str(), ios_base::binary );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( out, view( dst ), tag_t() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( stream_test_2 )
{
    filebuf in_buf;
    if( !in_buf.open( png_filename.c_str(), ios::in | ios::binary ) )
    {
        BOOST_CHECK( false );
    }

    istream in( &in_buf );

    rgba8_image_t img;
    read_image( in, img, tag_t() );
}

BOOST_AUTO_TEST_CASE( subimage_test )
{
    run_subimage_test< rgba8_image_t, tag_t >( png_filename
                                             , point_t(  0,  0 )
                                             , point_t( 50, 50 )
                                             );


    run_subimage_test< rgba8_image_t, tag_t >( png_filename
                                             , point_t( 135, 95 )
                                             , point_t(  50, 50 )
                                             );
}

BOOST_AUTO_TEST_CASE( dynamic_image_test )
{
    typedef mpl::vector< gray8_image_t
                       , gray16_image_t
                       , rgb8_image_t
                       , rgba8_image_t
                       > my_img_types;


    any_image< my_img_types > runtime_image;

    read_image( png_filename.c_str()
              , runtime_image
              , tag_t()
              );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( png_out + "dynamic_image_test.png"
              , view( runtime_image )
              , tag_t()
              );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
