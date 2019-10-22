//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_TEST_MODULE targa_test
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

#include <boost/gil.hpp>
#include <boost/gil/extension/io/targa.hpp>

#include <boost/test/unit_test.hpp>

#include <fstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

using namespace std;
using namespace boost;
using namespace gil;
namespace fs = boost::filesystem;

using tag_t = targa_tag;

BOOST_AUTO_TEST_SUITE( gil_io_targa_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( read_image_info_using_string )
{

    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( targa_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 124 );
        BOOST_CHECK_EQUAL( backend._info._height, 124 );
    }

    {
        ifstream in( targa_filename.c_str(), ios::binary );

        using backend_t = get_reader_backend<ifstream, tag_t>::type;

        backend_t backend = read_image_info( in
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 124 );
        BOOST_CHECK_EQUAL( backend._info._height, 124 );
    }

    {
        FILE* file = fopen( targa_filename.c_str(), "rb" );

        using backend_t = get_reader_backend<FILE*, tag_t>::type;

        backend_t backend = read_image_info( file
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 124 );
        BOOST_CHECK_EQUAL( backend._info._height, 124 );
    }

    {
        fs::path my_path( targa_filename );

        using backend_t = get_reader_backend<fs::path, tag_t>::type;

        backend_t backend = read_image_info( my_path
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 124 );
        BOOST_CHECK_EQUAL( backend._info._height, 124  );
    }
}

BOOST_AUTO_TEST_CASE( read_image_test )
{
    {
        rgb8_image_t img;
        read_image( targa_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 124 );
        BOOST_CHECK_EQUAL( img.height(), 124 );
    }

    {
        ifstream in( targa_filename.c_str(), ios::binary );

        rgb8_image_t img;
        read_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 124 );
        BOOST_CHECK_EQUAL( img.height(), 124 );
    }

    {
        FILE* file = fopen( targa_filename.c_str(), "rb" );

        rgb8_image_t img;
        read_image( file, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 124 );
        BOOST_CHECK_EQUAL( img.height(), 124 );
    }
}

BOOST_AUTO_TEST_CASE( read_and_convert_image_test )
{
    {
        rgb8_image_t img;
        read_and_convert_image( targa_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 124 );
        BOOST_CHECK_EQUAL( img.height(), 124 );
    }

    {
        ifstream in( targa_filename.c_str(), ios::binary );

        rgb8_image_t img;
        read_and_convert_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 124 );
        BOOST_CHECK_EQUAL( img.height(), 124 );
    }

    {
        FILE* file = fopen( targa_filename.c_str(), "rb" );

        rgb8_image_t img;
        read_and_convert_image( file, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 124 );
        BOOST_CHECK_EQUAL( img.height(), 124 );
    }
}

BOOST_AUTO_TEST_CASE( read_view_test )
{
    {
        rgb8_image_t img( 124, 124 );
        read_view( targa_filename, view( img ), tag_t() );
    }

    {
        ifstream in( targa_filename.c_str(), ios::binary );

        rgb8_image_t img( 124, 124 );
        read_view( in, view( img ), tag_t() );
    }

    {
        FILE* file = fopen( targa_filename.c_str(), "rb" );

        rgb8_image_t img( 124, 124 );
        read_view( file, view( img ), tag_t() );
    }
}

BOOST_AUTO_TEST_CASE( read_and_convert_view_test )
{
    {
        rgb8_image_t img( 124, 124 );
        read_and_convert_view( targa_filename, view( img ), tag_t() );
    }

    {
        ifstream in( targa_filename.c_str(), ios::binary );

        rgb8_image_t img( 124, 124 );
        read_and_convert_view( in, view( img ), tag_t() );
    }

    {
        FILE* file = fopen( targa_filename.c_str(), "rb" );

        rgb8_image_t img( 124, 124 );
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
        string filename( targa_out + "write_test_ofstream.tga" );

        ofstream out( filename.c_str(), ios::binary );

        write_view( out
                  , create_mandel_view( 124, 124
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( targa_out + "write_test_file.tga" );

        FILE* file = fopen( filename.c_str(), "wb" );

        write_view( file
                  , create_mandel_view( 124, 124
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        string filename( targa_out + "write_test_info.tga" );

        image_write_info< tag_t > info;

        FILE* file = fopen( filename.c_str(), "wb" );

        write_view( file
                  , create_mandel_view( 124, 124
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , info
                  );
    }
}
#endif //BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
BOOST_AUTO_TEST_CASE( stream_test )
{
    // 1. Read an image.
    ifstream in( targa_filename.c_str(), ios::binary );

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
    string filename( targa_out + "stream_test.tga" );
    ofstream out( filename.c_str(), ios_base::binary );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( out, view( dst ), tag_t() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( stream_test_2 )
{
    filebuf in_buf;
    if( !in_buf.open( targa_filename.c_str(), ios::in | ios::binary ) )
    {
        BOOST_CHECK( false );
    }

    istream in( &in_buf );

    rgb8_image_t img;
    read_image( in, img, tag_t() );
}

BOOST_AUTO_TEST_CASE( subimage_test )
{
    run_subimage_test< rgb8_image_t, tag_t >( targa_filename
                                            , point_t(  0,  0 )
                                            , point_t( 50, 50 )
                                            );

    // not working
    //run_subimage_test< rgb8_image_t, tag_t >( targa_filename
    //                                        , point_t( 39,  7 )
    //                                        , point_t( 50, 50 )
    //                                        );
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

    read_image( targa_filename.c_str()
              , runtime_image
              , tag_t()
              );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( targa_out + "dynamic_image_test.tga"
              , view( runtime_image )
              , tag_t()
              );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
