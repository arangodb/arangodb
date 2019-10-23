//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#define BOOST_FILESYSTEM_VERSION 3

#include <boost/gil.hpp>
#include <boost/gil/extension/io/bmp.hpp>

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <type_traits>

#include "paths.hpp"

using namespace std;
using namespace boost;
using namespace gil;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE( gil_io_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( make_reader_backend_test )
{
    {
        static_assert(std::is_same<gil::detail::is_supported_path_spec<char*>::type, mpl::true_>::value, "");

        get_reader_backend< const char*, bmp_tag >::type backend_char   = make_reader_backend( bmp_filename.c_str(), bmp_tag() );
        get_reader_backend< std::string, bmp_tag >::type backend_string = make_reader_backend( bmp_filename, bmp_tag() );

        FILE* file = fopen( bmp_filename.c_str(), "rb" );
        get_reader_backend< FILE*, bmp_tag >::type backend_file = make_reader_backend( file, bmp_tag() );

        ifstream in( bmp_filename.c_str(), ios::binary );
        get_reader_backend< std::ifstream, bmp_tag >::type backend_ifstream = make_reader_backend( in, bmp_tag() );

        fs::path my_path( bmp_filename );
        get_reader_backend< std::wstring, bmp_tag >::type backend_wstring = make_reader_backend( my_path.wstring(), bmp_tag() );
        get_reader_backend< fs::path    , bmp_tag >::type backend_path    = make_reader_backend( my_path          , bmp_tag() );
    }

    {
        get_reader_backend< const char*, bmp_tag >::type backend_char   = make_reader_backend( bmp_filename.c_str(), image_read_settings<bmp_tag>() );
        get_reader_backend< std::string, bmp_tag >::type backend_string = make_reader_backend( bmp_filename, image_read_settings<bmp_tag>() );

        FILE* file = fopen( bmp_filename.c_str(), "rb" );
        get_reader_backend< FILE*, bmp_tag >::type backend_file = make_reader_backend( file, image_read_settings<bmp_tag>() );

        ifstream in( bmp_filename.c_str(), ios::binary );
        get_reader_backend< std::ifstream, bmp_tag >::type backend_ifstream = make_reader_backend( in, image_read_settings<bmp_tag>() );

        fs::path my_path( bmp_filename );
        get_reader_backend< std::wstring, bmp_tag >::type backend_wstring = make_reader_backend( my_path.wstring(), image_read_settings<bmp_tag>() );
        get_reader_backend< fs::path, bmp_tag >::type backend_path    = make_reader_backend( my_path          , image_read_settings<bmp_tag>() );
    }
}

BOOST_AUTO_TEST_CASE( make_reader_test )
{
    {
        get_reader_backend< const char*, bmp_tag >::type reader_char   = make_reader( bmp_filename.c_str(), bmp_tag(), gil::detail::read_and_no_convert() );
        get_reader_backend< std::string, bmp_tag >::type reader_string = make_reader( bmp_filename, bmp_tag(), gil::detail::read_and_no_convert() );

        FILE* file = fopen( bmp_filename.c_str(), "rb" );
        get_reader_backend< FILE*, bmp_tag >::type reader_file = make_reader( file, bmp_tag(), gil::detail::read_and_no_convert() );

        ifstream in( bmp_filename.c_str(), ios::binary );
        get_reader_backend< std::ifstream, bmp_tag >::type reader_ifstream = make_reader( in, bmp_tag(), gil::detail::read_and_no_convert() );

        fs::path my_path( bmp_filename );
        get_reader_backend< std::wstring, bmp_tag >::type reader_wstring = make_reader( my_path.wstring(), bmp_tag(), gil::detail::read_and_no_convert() );
        get_reader_backend< fs::path    , bmp_tag >::type reader_path    = make_reader( my_path          , bmp_tag(), gil::detail::read_and_no_convert() );
    }

    {
        get_reader_backend< const char*, bmp_tag >::type reader_char   = make_reader( bmp_filename.c_str(), image_read_settings< bmp_tag >(), gil::detail::read_and_no_convert() );
        get_reader_backend< std::string, bmp_tag >::type reader_string = make_reader( bmp_filename, image_read_settings< bmp_tag >(), gil::detail::read_and_no_convert() );

        FILE* file = fopen( bmp_filename.c_str(), "rb" );
        get_reader_backend< FILE*, bmp_tag >::type reader_file = make_reader( file, image_read_settings< bmp_tag >(), gil::detail::read_and_no_convert() );

        ifstream in( bmp_filename.c_str(), ios::binary );
        get_reader_backend< std::ifstream, bmp_tag >::type reader_ifstream = make_reader( in, image_read_settings< bmp_tag >(), gil::detail::read_and_no_convert() );

        fs::path my_path( bmp_filename );
        get_reader_backend< std::wstring, bmp_tag >::type reader_wstring = make_reader( my_path.wstring(), image_read_settings< bmp_tag >(), gil::detail::read_and_no_convert() );
        get_reader_backend< fs::path, bmp_tag >::type reader_path    = make_reader( my_path          , image_read_settings< bmp_tag >(), gil::detail::read_and_no_convert() );
    }
}

BOOST_AUTO_TEST_CASE( make_dynamic_image_reader_test )
{
    {
        get_dynamic_image_reader< const char*, bmp_tag >::type reader_char   = make_dynamic_image_reader( bmp_filename.c_str(), bmp_tag() );
        get_dynamic_image_reader< std::string, bmp_tag >::type reader_string = make_dynamic_image_reader( bmp_filename, bmp_tag() );

        FILE* file = fopen( bmp_filename.c_str(), "rb" );
        get_dynamic_image_reader< FILE*, bmp_tag >::type reader_file = make_dynamic_image_reader( file, bmp_tag() );

        ifstream in( bmp_filename.c_str(), ios::binary );
        get_dynamic_image_reader< std::ifstream, bmp_tag >::type reader_ifstream = make_dynamic_image_reader( in, bmp_tag() );

        fs::path my_path( bmp_filename );
        get_dynamic_image_reader< std::wstring, bmp_tag >::type reader_wstring = make_dynamic_image_reader( my_path.wstring(), bmp_tag() );
        get_dynamic_image_reader< fs::path    , bmp_tag >::type reader_path    = make_dynamic_image_reader( my_path          , bmp_tag() );
    }

    {
        get_dynamic_image_reader< const char*, bmp_tag >::type reader_char   = make_dynamic_image_reader( bmp_filename.c_str(), image_read_settings< bmp_tag >() );
        get_dynamic_image_reader< std::string, bmp_tag >::type reader_string = make_dynamic_image_reader( bmp_filename, image_read_settings< bmp_tag >() );

        FILE* file = fopen( bmp_filename.c_str(), "rb" );
        get_dynamic_image_reader< FILE*, bmp_tag >::type reader_file = make_dynamic_image_reader( file, image_read_settings< bmp_tag >() );

        ifstream in( bmp_filename.c_str(), ios::binary );
        get_dynamic_image_reader< std::ifstream, bmp_tag >::type reader_ifstream = make_dynamic_image_reader( in, image_read_settings< bmp_tag >() );

        fs::path my_path( bmp_filename );
        get_dynamic_image_reader< std::wstring, bmp_tag >::type reader_wstring = make_dynamic_image_reader( my_path.wstring(), image_read_settings< bmp_tag >() );
        get_dynamic_image_reader< fs::path    , bmp_tag >::type reader_path    = make_dynamic_image_reader( my_path          , image_read_settings< bmp_tag >() );
    }
}

BOOST_AUTO_TEST_CASE( make_writer_test )
{
    {
        using writer_t = get_writer<char const*, bmp_tag>::type;

        static_assert(std::is_same<gil::detail::is_writer<writer_t>::type, boost::mpl::true_>::value, "");
    }

    {
        get_writer< const char*, bmp_tag >::type writer_char   = make_writer(( bmp_out + "make_test.bmp" ).c_str(), bmp_tag() );
        get_writer< std::string, bmp_tag >::type writer_string = make_writer(( bmp_out + "make_test.bmp" ), bmp_tag() );

        FILE* file = fopen(( bmp_out + "make_test.bmp" ).c_str(), "wb" );
        get_writer< FILE*, bmp_tag >::type  writer_file = make_writer( file, bmp_tag() );

        ofstream out(( bmp_out  + "make_test.bmp" ).c_str(), ios::binary );
        get_writer< std::ofstream, bmp_tag >::type writer_ofstream = make_writer( out, bmp_tag() );

        fs::path my_path( ( bmp_out  + "make_test.bmp" ).c_str() );
        get_writer< std::wstring, bmp_tag >::type writer_wstring = make_writer( my_path.wstring(), bmp_tag() );
        get_writer< fs::path    , bmp_tag >::type writer_path    = make_writer( my_path          , bmp_tag() );
    }

    {
        get_writer< const char*, bmp_tag >::type writer_char   = make_writer(( bmp_out + "make_test.bmp" ).c_str(), image_write_info< bmp_tag >() );
        get_writer< std::string, bmp_tag >::type writer_string = make_writer(( bmp_out + "make_test.bmp" )        , image_write_info< bmp_tag >() );

        FILE* file = fopen( (bmp_out + string( "make_test.bmp")).c_str(), "wb" );
        get_writer< FILE*, bmp_tag >::type writer_file = make_writer( file, image_write_info< bmp_tag >() );

        ofstream out( ( bmp_out  + "make_test.bmp" ).c_str(), ios::binary );
        get_writer< std::ofstream, bmp_tag >::type writer_ofstream = make_writer( out, image_write_info< bmp_tag >() );

        fs::path my_path( bmp_out  + "make_test.bmp" );
        get_writer< std::wstring, bmp_tag >::type writer_wstring = make_writer( my_path.wstring(), image_write_info< bmp_tag >() );
        get_writer< fs::path    , bmp_tag >::type writer_path    = make_writer( my_path          , image_write_info< bmp_tag >() );
    }
}

BOOST_AUTO_TEST_CASE( make_dynamic_image_writer_test )
{
    {
        get_dynamic_image_writer< const char*, bmp_tag >::type writer_char = make_dynamic_image_writer( (bmp_out + string( "make_test.bmp")).c_str(), bmp_tag() );
        get_dynamic_image_writer< std::string, bmp_tag >::type writer_string = make_dynamic_image_writer( bmp_out  + "make_test.bmp", bmp_tag() );

        FILE* file = fopen( (bmp_out + string( "make_test.bmp")).c_str(), "wb" );
        get_dynamic_image_writer< FILE*, bmp_tag >::type writer_file = make_dynamic_image_writer( file, bmp_tag() );

        ofstream out(( bmp_out  + "make_test.bmp" ).c_str(), ios::binary );
        get_dynamic_image_writer< std::ofstream, bmp_tag >::type writer_ofstream = make_dynamic_image_writer( out, bmp_tag() );

        fs::path my_path( bmp_out  + "make_test.bmp" );
        get_dynamic_image_writer< std::wstring, bmp_tag >::type writer_wstring = make_dynamic_image_writer( my_path.wstring(), bmp_tag() );
        get_dynamic_image_writer< fs::path    , bmp_tag >::type writer_path    = make_dynamic_image_writer( my_path          , bmp_tag() );
    }

    {
        get_dynamic_image_writer< const char*, bmp_tag >::type writer_char   = make_dynamic_image_writer( (bmp_out + string( "make_test.bmp")).c_str(), image_write_info< bmp_tag >() );
        get_dynamic_image_writer< std::string, bmp_tag >::type writer_string = make_dynamic_image_writer( bmp_out  + "make_test.bmp", image_write_info< bmp_tag >() );

        FILE* file = fopen( (bmp_out + string( "make_test.bmp")).c_str(), "wb" );
        get_dynamic_image_writer< FILE*, bmp_tag >::type writer_file = make_dynamic_image_writer( file, image_write_info< bmp_tag >() );

        ofstream out(( bmp_out  + "make_test.bmp" ).c_str(), ios::binary );
        get_dynamic_image_writer< std::ofstream, bmp_tag >::type writer_ofstream = make_dynamic_image_writer( out, image_write_info< bmp_tag >() );

        fs::path my_path( bmp_out  + "make_test.bmp" );
        get_dynamic_image_writer< std::wstring, bmp_tag >::type writer_wstring = make_dynamic_image_writer( my_path.wstring(), image_write_info< bmp_tag >() );
        get_dynamic_image_writer< fs::path    , bmp_tag >::type writer_path    = make_dynamic_image_writer( my_path          , image_write_info< bmp_tag >() );
    }
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
