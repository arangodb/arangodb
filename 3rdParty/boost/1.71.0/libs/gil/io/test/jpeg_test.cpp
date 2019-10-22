//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_TEST_MODULE jpeg_test
#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

#include <boost/gil.hpp>

#include <boost/gil/extension/io/jpeg.hpp>

#include <boost/test/unit_test.hpp>

#include <fstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

using namespace boost;
using namespace gil;

using tag_t = jpeg_tag;

BOOST_AUTO_TEST_SUITE( gil_io_jpeg_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( read_image_info_test )
{

    {
        using backend_t = get_reader_backend
            <
                std::string const,
                tag_t
            >::type;

        backend_t backend = read_image_info( jpeg_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }

    {
        std::ifstream in( jpeg_filename.c_str(), ios::binary );

        using backend_t = get_reader_backend<std::ifstream, tag_t>::type;

        backend_t backend = read_image_info( in
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }

    {
        FILE* file = fopen( jpeg_filename.c_str(), "rb" );

        using backend_t = get_reader_backend<FILE*, tag_t>::type;

        backend_t backend = boost::gil::read_image_info( file
                                                       , tag_t()
                                                       );

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }

    {
        using backend_t = get_reader_backend<boost::filesystem::path, tag_t>::type;

        backend_t backend =
            boost::gil::read_image_info(
                 boost::filesystem::path(jpeg_filename),
                 tag_t());

        BOOST_CHECK_EQUAL( backend._info._width , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height,  600u );
    }
}

BOOST_AUTO_TEST_CASE( read_image_test )
{
    {
        rgb8_image_t img;
        read_image( jpeg_filename, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        std::ifstream in( jpeg_filename.c_str(), ios::binary );

        rgb8_image_t img;
        read_image( in, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        FILE* file = fopen( jpeg_filename.c_str(), "rb" );

        rgb8_image_t img;
        read_image( file, img, tag_t() );

        BOOST_CHECK_EQUAL( img.width() , 1000u );
        BOOST_CHECK_EQUAL( img.height(),  600u );
    }

    {
        rgb8_image_t img;

        image_read_settings< jpeg_tag > settings( point_t(  0,  0 )
                                                , point_t( 10, 10 )
                                                , jpeg_dct_method::slow
                                                );

        read_image( jpeg_filename, img, settings );

        BOOST_CHECK_EQUAL( img.width() , 10u );
        BOOST_CHECK_EQUAL( img.height(), 10u );
    }

}

BOOST_AUTO_TEST_CASE( read_and_convert_image_test )
{
    {
        rgb8_image_t img;
        read_and_convert_image( jpeg_filename, img, tag_t() );
    }

    {
        std::ifstream in( jpeg_filename.c_str(), ios::binary );

        rgb8_image_t img;
        read_and_convert_image( in, img, tag_t() );
    }
}

BOOST_AUTO_TEST_CASE( read_view_test )
{
    {
        rgb8_image_t img( 1000, 600 );
        read_view( jpeg_filename, view( img ), tag_t() );
    }

    {
        std::ifstream in( jpeg_filename.c_str(), ios::binary );

        rgb8_image_t img( 1000, 600 );
        read_view( in, view( img ), tag_t() );
    }

    {
        FILE* file = fopen( jpeg_filename.c_str(), "rb" );

        rgb8_image_t img( 1000, 600 );
        read_view( file, view( img ), tag_t() );
    }
}
BOOST_AUTO_TEST_CASE( read_and_convert_view_test )
{
    {
        rgb8_image_t img( 1000, 600 );
        read_and_convert_view( jpeg_filename, view( img ), tag_t() );
    }

    {
        std::ifstream in( jpeg_filename.c_str(), ios::binary );

        rgb8_image_t img( 1000, 600 );
        read_and_convert_view( in, view( img ), tag_t() );
    }

    {
        FILE* file = fopen( jpeg_filename.c_str(), "rb" );

        rgb8_image_t img( 1000, 600 );
        read_and_convert_view( file
                             , view( img )
                             , tag_t()
                             );
    }
}

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
BOOST_AUTO_TEST_CASE( write_view_test )
{
    {
        std::string filename( jpeg_out + "write_test_string.jpg" );

        write_view( filename
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        std::string filename( jpeg_out + "write_test_ofstream.jpg" );
        std::ofstream out( filename.c_str(), ios::binary );

        write_view( out
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , tag_t()
                  );
    }

    {
        std::string filename( jpeg_out + "write_test_file.jpg" );
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
        std::string filename( jpeg_out + "write_test_info.jpg" );
        FILE* file = fopen( filename.c_str(), "wb" );

        image_write_info< jpeg_tag > info;
        write_view( file
                  , create_mandel_view( 320, 240
                                      , rgb8_pixel_t( 0,   0, 255 )
                                      , rgb8_pixel_t( 0, 255,   0 )
                                      )
                  , info
                  );
    }
}
#endif //BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

BOOST_AUTO_TEST_CASE( stream_test )
{
    // 1. Read an image.
    std::ifstream in( jpeg_filename.c_str(), ios::binary );

    rgb8_image_t img;
    read_image( in, img, tag_t() );

    // 2. Write image to in-memory buffer.
    std::stringstream out_buffer( ios_base::in | ios_base::out | ios_base::binary );
    write_view( out_buffer, view( img ), tag_t() );

    // 3. Copy in-memory buffer to another.
    std::stringstream in_buffer( ios_base::in | ios_base::out | ios_base::binary );
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    rgb8_image_t dst;
    read_image( in_buffer, dst, tag_t() );

    // 5. Write out image.
    std::string filename( jpeg_out + "stream_test.jpg" );
    std::ofstream out( filename.c_str(), ios_base::binary );
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( out, view( dst ), tag_t() );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( stream_test_2 )
{
    std::filebuf in_buf;
    if( !in_buf.open( jpeg_filename.c_str(), ios::in | ios::binary ) )
    {
        BOOST_CHECK( false );
    }

    std::istream in( &in_buf );

    rgb8_image_t img;
    read_image( in, img, tag_t() );
}

BOOST_AUTO_TEST_CASE( subimage_test )
{
    run_subimage_test< rgb8_image_t, tag_t >( jpeg_filename
                                            , point_t(  0,  0 )
                                            , point_t( 50, 50 )
                                            );

    run_subimage_test< rgb8_image_t, tag_t >( jpeg_filename
                                            , point_t( 43, 24 )
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

    read_image( jpeg_filename.c_str()
              , runtime_image
              , jpeg_tag()
              );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( jpeg_out + "old_dynamic_image_test.jpg"
              , view( runtime_image )
              , jpeg_tag()
              );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
