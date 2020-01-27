//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_TEST_MODULE tiff_test
#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

#include <boost/gil/extension/io/tiff.hpp>

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <sstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

// This test file will only test the library's interface.
// It's more of a compile time test than a runtime test.

using namespace std;
using namespace boost;
using namespace gil;
namespace fs = boost::filesystem;

using tag_t = tiff_tag;

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( read_image_info_test )
{
    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( tiff_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }

    {
        ifstream in( tiff_filename.c_str(), ios::binary );

        using backend_t = get_reader_backend<ifstream, tag_t>::type;

        backend_t backend = read_image_info( in
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }

    {
        TIFF* file = TIFFOpen( tiff_filename.c_str(), "r" );

        using backend_t = get_reader_backend<FILE*, tag_t>::type;

        backend_t backend = read_image_info( file
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }

    {
        fs::path my_path( tiff_filename );

        using backend_t = get_reader_backend<fs::path, tag_t>::type;

        backend_t backend = read_image_info( my_path
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
        read_image( tiff_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {

        ifstream in( tiff_filename.c_str(), ios::binary );

        rgba8_image_t img;
        read_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        TIFF* file = TIFFOpen( tiff_filename.c_str(), "r" );

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
        read_and_convert_image( tiff_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        ifstream in( tiff_filename.c_str(), ios::binary );

        rgb8_image_t img;
        read_and_convert_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        TIFF* file = TIFFOpen( tiff_filename.c_str(), "r" );

        rgb8_image_t img;
        read_and_convert_image( file, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }
}

BOOST_AUTO_TEST_CASE( read_and_convert_image_test_2 )
{
    gray8_image_t img;
    read_and_convert_image( tiff_filename, img, tag_t() );

    rgba8_image_t img2;
    read_image( tiff_filename, img2, tag_t() );


    BOOST_CHECK( equal_pixels( const_view( img )
                             , color_converted_view< gray8_pixel_t>( const_view( img2 ) )
                             )
               );
}

BOOST_AUTO_TEST_CASE( read_view_test )
{
    {
        rgba8_image_t img( 1000, 600 );
        read_view( tiff_filename, view( img ), tag_t() );
    }

    {
        ifstream in( tiff_filename.c_str(), ios::binary );

        rgba8_image_t img( 1000, 600 );
        read_view( in, view( img ), tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        TIFF* file = TIFFOpen( tiff_filename.c_str(), "r" );

        rgba8_image_t img( 1000, 600 );
        read_view( file, view( img ), tag_t() );
    }
}

BOOST_AUTO_TEST_CASE( read_and_convert_view_test )
{
    {
        rgb8_image_t img( 1000, 600 );
        read_and_convert_view( tiff_filename, view( img ), tag_t() );
    }

    {
        ifstream in( tiff_filename.c_str(), ios::binary );

        rgb8_image_t img( 1000, 600 );
        read_and_convert_view( in, view( img ), tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        TIFF* file = TIFFOpen( tiff_filename.c_str(), "r" );

        rgb8_image_t img( 1000, 600 );
        read_and_convert_view( file, view( img ), tag_t() );
    }
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
BOOST_AUTO_TEST_CASE( write_view_test )
{
    {
        string filename( tiff_out + "write_test_string.tif" );

        write_view( filename
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( tiff_out + "write_test_ofstream.tif" );
        ofstream out( filename.c_str(), ios_base::binary );

        write_view( out
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( tiff_out + "write_test_tiff.tif" );
        TIFF* file = TIFFOpen( filename.c_str(), "w" );

        write_view( file
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( tiff_out + "write_test_info.tif" );

        image_write_info< tiff_tag > info;
        write_view( filename
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
    ifstream in( tiff_filename.c_str(), ios::binary );

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
    string filename( tiff_out + "stream_test.tif" );
    ofstream out( filename.c_str(), ios_base::binary );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( out, view( dst ), tag_t() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( stream_test_2 )
{
    filebuf in_buf;
    if( !in_buf.open( tiff_filename.c_str(), ios::in | ios::binary ) )
    {
        BOOST_CHECK( false );
    }

    istream in( &in_buf );

    rgba8_image_t img;
    read_image( in, img, tag_t() );
}

BOOST_AUTO_TEST_CASE( subimage_test )
{
    run_subimage_test< rgba8_image_t, tag_t >( tiff_filename
                                             , point_t(  0,  0 )
                                             , point_t( 50, 50 )
                                             );

    run_subimage_test< rgba8_image_t, tag_t >( tiff_filename
                                             , point_t(  50,  50 )
                                             , point_t(  50,  50 )
                                             );
}

BOOST_AUTO_TEST_CASE( dynamic_image_test )
{
    // This test has been disabled for now because of
    // compilation issues with MSVC10.

    using my_img_types = mpl::vector
        <
            gray8_image_t,
            gray16_image_t,
            rgb8_image_t,
            gray1_image_t
        >;

    any_image< my_img_types > runtime_image;

    read_image( tiff_filename.c_str()
              , runtime_image
              , tag_t()
              );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( tiff_out + "dynamic_image_test.tif"
              , view( runtime_image )
              , tag_t()
              );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
