//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_TEST_MODULE bmp_test
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/gil.hpp>
#include <boost/gil/extension/io/bmp.hpp>

#include <boost/test/unit_test.hpp>

#include <fstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

using namespace std;
using namespace boost;
using namespace gil;
namespace fs = boost::filesystem;

using tag_t = bmp_tag;

BOOST_AUTO_TEST_SUITE( gil_io_bmp_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( read_image_info_using_string )
{
    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( bmp_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000 );
        BOOST_CHECK_EQUAL( backend._info._height, 600  );
    }

    {
        ifstream in( bmp_filename.c_str(), ios::binary );

        using backend_t = get_reader_backend<std::ifstream, tag_t>::type;

        backend_t backend = read_image_info( in
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000 );
        BOOST_CHECK_EQUAL( backend._info._height, 600  );
    }

    {
        FILE* file = fopen( bmp_filename.c_str(), "rb" );

        using backend_t = get_reader_backend<FILE*, tag_t>::type;

        backend_t backend = read_image_info( file
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000 );
        BOOST_CHECK_EQUAL( backend._info._height, 600  );
    }

    {
        fs::path my_path( bmp_filename );

        using backend_t = get_reader_backend<fs::path, tag_t>::type;

        backend_t backend = read_image_info( my_path
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000 );
        BOOST_CHECK_EQUAL( backend._info._height, 600   );
    }
}

BOOST_AUTO_TEST_CASE( read_image_test )
{
    {
        rgb8_image_t img;
        read_image( bmp_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000 );
        BOOST_CHECK_EQUAL( img.height(), 600  );
    }

    {
        ifstream in( bmp_filename.c_str(), ios::binary );

        rgb8_image_t img;
        read_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000 );
        BOOST_CHECK_EQUAL( img.height(), 600  );
    }

    {
        FILE* file = fopen( bmp_filename.c_str(), "rb" );

        rgb8_image_t img;
        read_image( file, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000 );
        BOOST_CHECK_EQUAL( img.height(), 600  );
    }

    {
        fs::path my_path( bmp_filename );

        rgb8_image_t img;
        read_image( my_path, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000 );
        BOOST_CHECK_EQUAL( img.height(), 600  );
    }
}

BOOST_AUTO_TEST_CASE( read_and_convert_image_test )
{
    {
        rgb8_image_t img;
        read_and_convert_image( bmp_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000 );
        BOOST_CHECK_EQUAL( img.height(), 600  );
    }

    {
        ifstream in( bmp_filename.c_str(), ios::binary );

        rgb8_image_t img;
        read_and_convert_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000 );
        BOOST_CHECK_EQUAL( img.height(), 600  );
    }

    {
        FILE* file = fopen( bmp_filename.c_str(), "rb" );

        rgb8_image_t img;
        read_and_convert_image( file, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000 );
        BOOST_CHECK_EQUAL( img.height(), 600  );
    }
}

BOOST_AUTO_TEST_CASE( read_view_test )
{
    {
        rgb8_image_t img( 1000, 600  );
        read_view( bmp_filename, view( img ), tag_t() );
    }

    {
        ifstream in( bmp_filename.c_str(), ios::binary );

        rgb8_image_t img( 1000, 600  );
        read_view( in, view( img ), tag_t() );
    }

    {
        FILE* file = fopen( bmp_filename.c_str(), "rb" );

        rgb8_image_t img( 1000, 600  );
        read_view( file, view( img ), tag_t() );
    }
}

BOOST_AUTO_TEST_CASE( read_and_convert_view_test )
{
    {
        rgb8_image_t img( 1000, 600  );
        read_and_convert_view( bmp_filename, view( img ), tag_t() );
    }

    {
        ifstream in( bmp_filename.c_str(), ios::binary );

        rgb8_image_t img( 1000, 600  );
        read_and_convert_view( in, view( img ), tag_t() );
    }

    {
        FILE* file = fopen( bmp_filename.c_str(), "rb" );

        rgb8_image_t img( 1000, 600  );
        read_and_convert_view( file
                             , view( img )
                             , tag_t()
                             );
    }
}

BOOST_AUTO_TEST_CASE( write_view_test )
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    {
        string filename( bmp_out + "write_test_string.bmp" );

        write_view( filename
                  , create_mandel_view( 1000, 600
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( bmp_out + "write_test_ofstream.bmp" );

        ofstream out( filename.c_str(), ios::binary );

        write_view( out
                  , create_mandel_view( 1000, 600
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( bmp_out + "write_test_file.bmp" );

        FILE* file = fopen( filename.c_str(), "wb" );

        write_view( file
                  , create_mandel_view( 1000, 600
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( bmp_out + "write_test_info.bmp" );

        image_write_info< tag_t > info;

        FILE* file = fopen( filename.c_str(), "wb" );

        write_view( file
                  , create_mandel_view( 1000, 600
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , info
                  );
    }

#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( stream_test )
{
    // 1. Read an image.
    ifstream in( bmp_filename.c_str(), ios::binary );

    rgb8_image_t img;
    read_image( in, img, tag_t() );

    // 2. Write image to in-memory buffer.
    stringstream out_buffer( ios_base::in | ios_base::out | ios_base::binary );
    write_view( out_buffer, view( img ), tag_t() );

    // 3. Copy in-memory buffer to another.
    stringstream in_buffer( ios_base::in | ios_base::out | ios_base::binary );
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    rgb8_image_t dst;
    read_image( in_buffer, dst, tag_t() );

    // 5. Write out image.
    string filename( bmp_out + "stream_test.bmp" );
    ofstream out( filename.c_str(), ios_base::binary );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( out, view( dst ), tag_t() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( stream_test_2 )
{
    filebuf in_buf;
    if( !in_buf.open( bmp_filename.c_str(), ios::in | ios::binary ) )
    {
        BOOST_CHECK( false );
    }

    istream in( &in_buf );

    rgb8_image_t img;
    read_image( in, img, tag_t() );
}

BOOST_AUTO_TEST_CASE( subimage_test )
{
    run_subimage_test< rgb8_image_t, tag_t >( bmp_filename
                                            , point_t(   0, 0 )
                                            , point_t( 1000, 1 )
                                            );

    run_subimage_test< rgb8_image_t, tag_t >( bmp_filename
                                            , point_t( 39,  7 )
                                            , point_t( 50, 50 )
                                            );
}

BOOST_AUTO_TEST_CASE( dynamic_image_test )
{
    using my_img_types = mpl::vector
        <
            gray8_image_t,
            gray16_image_t,
            rgb8_image_t,
            rgba8_image_t
        >;

    any_image< my_img_types > runtime_image;

    read_image( bmp_filename.c_str()
              , runtime_image
              , tag_t()
              );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( bmp_out + "dynamic_image_test.bmp"
              , view( runtime_image )
              , tag_t()
              );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
