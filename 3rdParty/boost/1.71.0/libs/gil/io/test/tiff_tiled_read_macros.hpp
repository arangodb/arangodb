//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TIFF_TILED_READ_MACROS_HPP
#define BOOST_GIL_TIFF_TILED_READ_MACROS_HPP

#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff/read.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/comparison/less.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#include "paths.hpp"

using tag_t = boost::gil::tiff_tag;

#define GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT( \
                                BOOST_PP_CAT( \
                                    BOOST_PP_CAT( \
                                        BOOST_PP_CAT( \
                                            BOOST_PP_CAT( read_tile_and_compare_with_ \
                                                        , BOOST_PP_TUPLE_ELEM(2,0,data) \
                                                        ) \
                                                , BOOST_PP_TUPLE_ELEM(2,1,data) \
                                                ) \
                                            ,_strip_ \
                                            ) \
                                        ,n \
                                        ) \
                                      , bit_bit_aligned \
                                      ) \
                        )\
    { \
      using namespace std; \
      using namespace boost; \
      using namespace gil; \
      string filename_strip( tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,0,data)) + "-strip-" + BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,1,data)) + "-" ); \
      string filename_tile ( tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,0,data)) + "-tile-"  + BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,1,data)) + "-" ); \
      string padding(""); \
      if(BOOST_PP_LESS(n, 10)==1) \
        padding = "0"; \
      filename_strip = filename_strip + padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      filename_tile  = filename_tile  + padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      bit_aligned_image3_type< n, n, n, rgb_layout_t >::type img_strip, img_tile; \
      read_image( filename_strip, img_strip, tag_t() ); \
      read_image( filename_tile,  img_tile,  tag_t() ); \
      BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true); \
    } \

// Special case for minisblack images
#define GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(read_tile_and_compare_with_,data),_strip_),n), bit_bit_aligned) )\
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
      bit_aligned_image1_type< n, gray_layout_t >::type img_strip, img_tile; \
      read_image( filename_strip, img_strip, tag_t() ); \
      read_image( filename_tile,  img_tile,  tag_t() ); \
      BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true); \
    } \

// Special case for palette images
#define GENERATE_TILE_STRIP_COMPARISON_PALETTE(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(read_tile_and_compare_with_,data),_strip_),n), bit) )\
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
      rgb16_image_t img_strip, img_tile; \
      read_image( filename_strip, img_strip, tag_t() ); \
      read_image( filename_tile,  img_tile,  tag_t() ); \
      BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true); \
    } \



#endif // BOOST_GIL_TIFF_TILED_READ_MACROS_HPP
