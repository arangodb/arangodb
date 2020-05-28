//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE pnm_read_test_module

#include <boost/gil/extension/io/pnm.hpp>

#include <boost/test/unit_test.hpp>

#include "paths.hpp"
#include "scanline_read_test.hpp"

using namespace std;
using namespace boost::gil;

using tag_t = pnm_tag;

BOOST_AUTO_TEST_SUITE( gil_io_pnm_tests )

#ifdef BOOST_GIL_IO_USE_PNM_TEST_SUITE_IMAGES

template< typename Image >
void write( Image&        img
          , const string& file_name
          )
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( pnm_out + file_name
              , view( img )
              , tag_t()
              );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

template< typename Image >
void test_pnm_scanline_reader( string filename )
{
    test_scanline_reader<Image, pnm_tag>( string( pnm_in + filename ).c_str() );
}

BOOST_AUTO_TEST_CASE( read_header_test )
{
    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( pnm_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._type     , pnm_image_type::color_asc_t::value );
        BOOST_CHECK_EQUAL( backend._info._width    , 256u                               );
        BOOST_CHECK_EQUAL( backend._info._height   , 256u                               );
        BOOST_CHECK_EQUAL( backend._info._max_value, 255u                               );
    }
}

BOOST_AUTO_TEST_CASE( read_reference_images_test )
{
    // p1.pnm
    {
        gray8_image_t img;

        read_image( pnm_in + "p1.pnm", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 200u );
        BOOST_CHECK_EQUAL( view( img ).height(), 200u );

        write( img, "p1.pnm" );

        test_pnm_scanline_reader< gray8_image_t >( "p1.pnm" );
    }

    // p2.pnm
    {
        gray8_image_t img;

        read_image( pnm_in + "p2.pnm", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 200u );
        BOOST_CHECK_EQUAL( view( img ).height(), 200u );

        write( img, "p2.pnm" );

        test_pnm_scanline_reader< gray8_image_t >( "p2.pnm" );
    }

    // p3.pnm
    {
        rgb8_image_t img;

        read_image( pnm_in + "p3.pnm", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 256u );
        BOOST_CHECK_EQUAL( view( img ).height(), 256u );

        write( img, "p3.pnm" );

        test_pnm_scanline_reader< rgb8_image_t >( "p3.pnm" );
    }

    // p4.pnm
    {
        gray1_image_t img;

        read_image( pnm_in + "p4.pnm", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 200u );
        BOOST_CHECK_EQUAL( view( img ).height(), 200u );

        write( img, "p4.pnm" );

        test_pnm_scanline_reader< gray1_image_t >( "p4.pnm" );
    }

    // p5.pnm
    {
        gray8_image_t img;

        read_image( pnm_in + "p5.pnm", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 200u );
        BOOST_CHECK_EQUAL( view( img ).height(), 200u );

        write( img, "p5.pnm" );

        test_pnm_scanline_reader< gray8_image_t >( "p5.pnm" );
    }

    // p6.pnm
    {
        rgb8_image_t img;

        read_image( pnm_in + "p6.pnm", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 256u );
        BOOST_CHECK_EQUAL( view( img ).height(), 256u );

        write( img, "p6.pnm" );

        test_pnm_scanline_reader< rgb8_image_t >( "p6.pnm" );
    }
}

#endif // BOOST_GIL_IO_USE_PNM_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
