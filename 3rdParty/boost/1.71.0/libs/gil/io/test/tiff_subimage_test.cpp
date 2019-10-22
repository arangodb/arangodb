//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE tiff_subimage_test_module

#include <boost/gil/extension/io/tiff.hpp>

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <sstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

using namespace std;
using namespace boost;
using namespace gil;

using tag_t = tiff_tag;

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/comparison/less.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#define GENERATE_SUBIMAGE_TEST(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(subimage_test,data),n), bit_bit_aligned) )\
    { \
      using namespace std; \
      using namespace boost; \
      using namespace gil; \
      string filename_strip( tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(data) + "-strip-" ); \
      string filename_tile ( tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(data) + "-tile-"  ); \
      string padding(""); \
      if(BOOST_PP_LESS(n, 10)==1) \
        padding = "0"; \
      filename_strip = filename_strip + padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      filename_tile  = filename_tile  + padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      bit_aligned_image1_type< n, gray_layout_t >::type img1, img2, img3; \
      point_t top_left( 10, 10 ); \
      point_t dim( 32, 32 ); \
      image_read_settings< tag_t > settings( top_left, dim ); \
      read_image( filename_strip, img1, settings ); \
      read_image( filename_tile, img2 , settings ); \
      read_image( filename_strip, img3, tag_t() ); \
      BOOST_CHECK( equal_pixels( const_view( img1 ), const_view( img2 ))); \
      BOOST_CHECK( equal_pixels( const_view( img1 ), subimage_view( view( img3 ), top_left, dim ))); \
    } \

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_PP_REPEAT_FROM_TO(  1,  8, GENERATE_SUBIMAGE_TEST, minisblack )
BOOST_PP_REPEAT_FROM_TO(  9, 16, GENERATE_SUBIMAGE_TEST, minisblack )
BOOST_PP_REPEAT_FROM_TO( 17, 27, GENERATE_SUBIMAGE_TEST, minisblack )
// @todo: there is a bug somewhere when the number of bits is 27 up to 31.


BOOST_AUTO_TEST_CASE( subimage_test_8 )
{
    gray8_image_t img1, img2, img3;

    point_t top_left( 10, 10 );
    point_t dim( 32, 32 );

    image_read_settings< tag_t > settings( top_left, dim );

    read_image( tiff_in_GM + "tiger-minisblack-strip-08.tif", img1, settings );
    read_image( tiff_in_GM + "tiger-minisblack-tile-08.tif" , img2, settings );
    read_image( tiff_in_GM + "tiger-minisblack-strip-08.tif", img3, tag_t()  );

    BOOST_CHECK( equal_pixels( const_view( img1 ), const_view( img2 )));
    BOOST_CHECK( equal_pixels( const_view( img1 ), subimage_view( view( img3 ), top_left, dim )));
}

BOOST_AUTO_TEST_CASE( subimage_test_16 )
{
    gray16_image_t img1, img2, img3;

    point_t top_left( 10, 10 );
    point_t dim( 32, 32 );

    image_read_settings< tag_t > settings( top_left, dim );

    read_image( tiff_in_GM + "tiger-minisblack-strip-16.tif", img1, settings );
    read_image( tiff_in_GM + "tiger-minisblack-tile-16.tif" , img2, settings );
    read_image( tiff_in_GM + "tiger-minisblack-strip-16.tif", img3, tag_t()  );

    BOOST_CHECK( equal_pixels( const_view( img1 ), const_view( img2 )));
    BOOST_CHECK( equal_pixels( const_view( img1 ), subimage_view( view( img3 ), top_left, dim )));
}

BOOST_AUTO_TEST_CASE( subimage_test_32 )
{
    using gray32_pixel_t = pixel<unsigned int, gray_layout_t>;
    image< gray32_pixel_t, false > img1, img2, img3;

    point_t top_left( 10, 10 );
    point_t dim( 32, 32 );

    image_read_settings< tag_t > settings( top_left, dim );

    read_image( tiff_in_GM + "tiger-minisblack-strip-32.tif", img1, settings );
    read_image( tiff_in_GM + "tiger-minisblack-tile-32.tif" , img2, settings );
    read_image( tiff_in_GM + "tiger-minisblack-strip-32.tif", img3, tag_t()  );

    BOOST_CHECK( equal_pixels( const_view( img1 ), const_view( img2 )));
    BOOST_CHECK( equal_pixels( const_view( img1 ), subimage_view( view( img3 ), top_left, dim )));
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
