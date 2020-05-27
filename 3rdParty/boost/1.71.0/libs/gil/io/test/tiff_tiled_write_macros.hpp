//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TIFF_TILED_WRITE_MACROS_HPP
#define BOOST_GIL_TIFF_TILED_WRITE_MACROS_HPP

#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/comparison/less.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#include "paths.hpp"

using tag_t = boost::gil::tiff_tag;

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#define GENERATE_WRITE_TILE_BIT_ALIGNED_RGB(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(write_rgb_tile_and_compare_with_,data),_strip_),n), bit_bit_aligned) )\
    { \
      using namespace std; \
      using namespace boost; \
      using namespace gil; \
      string filename( string( "tiger-" ) + BOOST_PP_STRINGIZE(data) + "-strip-contig-" ); \
      string path( tiff_in_GM ); \
      string padding(""); \
      if(BOOST_PP_LESS(n, 10)==1) \
        padding = "0"; \
      filename += padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      path += filename; \
      bit_aligned_image3_type< n, n, n, rgb_layout_t >::type img_strip, img_saved; \
      read_image( path, img_strip, tag_t() ); \
      image_write_info<tag_t> info; \
      info._is_tiled = true; \
      info._tile_width = info._tile_length = 16; \
      write_view( tiff_out + filename, view(img_strip), info ); \
      read_image( tiff_out + filename, img_saved, tag_t() ); \
      BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_saved) ), true); \
    } \

// Special case for minisblack images
#define GENERATE_WRITE_TILE_BIT_ALIGNED_MINISBLACK(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(write_minisblack_tile_and_compare_with_,data),_strip_),n), bit_bit_aligned) )\
    { \
      using namespace std; \
      using namespace boost; \
      using namespace gil; \
      string filename( string( "tiger-" ) + BOOST_PP_STRINGIZE(data) + "-strip-" ); \
      string path( tiff_in_GM ); \
      string padding(""); \
      if(BOOST_PP_LESS(n, 10)==1) \
        padding = "0"; \
      filename = filename + padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      path += filename; \
      bit_aligned_image1_type< n, gray_layout_t >::type img_strip, img_saved; \
      read_image( path, img_strip, tag_t() ); \
      image_write_info<tag_t> info; \
      info._is_tiled = true; \
      info._tile_width = info._tile_length = 16; \
      write_view( tiff_out + filename, view(img_strip), info ); \
      read_image( tiff_out + filename, img_saved, tag_t() ); \
      BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_saved) ), true); \
    } \

// Special case for palette images
#define GENERATE_WRITE_TILE_BIT_ALIGNED_PALETTE(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(write_palette_tile_and_compare_with_,data),_strip_),n), bit) )\
    { \
      using namespace std; \
      using namespace boost; \
      using namespace gil; \
      string filename( string( "tiger-" ) + BOOST_PP_STRINGIZE(data) + "-strip-" ); \
      string path( tiff_in_GM ); \
      string padding(""); \
      if(BOOST_PP_LESS(n, 10)==1) \
        padding = "0"; \
      filename = filename + padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      path += filename; \
      rgb16_image_t img_strip, img_saved; \
      read_image( path, img_strip, tag_t() ); \
      image_write_info<tag_t> info; \
      info._is_tiled = true; \
      info._tile_width = info._tile_length = 16; \
      write_view( tiff_out + filename, view(img_strip), info ); \
      read_image( tiff_out + filename, img_saved, tag_t() ); \
      BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_saved) ), true); \
    } \

#else

#define GENERATE_WRITE_TILE_BIT_ALIGNED_RGB(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(write_rgb_tile_and_compare_with_,data),_strip_),n), bit_bit_aligned) )\
    { \
      using namespace std; \
      using namespace boost; \
      using namespace gil; \
      string filename( string( "tiger-" ) + BOOST_PP_STRINGIZE(data) + "-strip-contig-" ); \
      string path( tiff_in_GM ); \
      string padding(""); \
      if(BOOST_PP_LESS(n, 10)==1) \
        padding = "0"; \
      filename += padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      path += filename; \
      bit_aligned_image3_type< n, n, n, rgb_layout_t >::type img_strip, img_saved; \
      read_image( path, img_strip, tag_t() ); \
      image_write_info<tag_t> info; \
      info._is_tiled = true; \
      info._tile_width = info._tile_length = 16; \
    } \

// Special case for minisblack images
#define GENERATE_WRITE_TILE_BIT_ALIGNED_MINISBLACK(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(write_minisblack_tile_and_compare_with_,data),_strip_),n), bit_bit_aligned) )\
    { \
      using namespace std; \
      using namespace boost; \
      using namespace gil; \
      string filename( string( "tiger-" ) + BOOST_PP_STRINGIZE(data) + "-strip-" ); \
      string path( tiff_in_GM ); \
      string padding(""); \
      if(BOOST_PP_LESS(n, 10)==1) \
        padding = "0"; \
      filename = filename + padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      path += filename; \
      bit_aligned_image1_type< n, gray_layout_t >::type img_strip, img_saved; \
      read_image( path, img_strip, tag_t() ); \
      image_write_info<tag_t> info; \
      info._is_tiled = true; \
      info._tile_width = info._tile_length = 16; \
    } \

// Special case for palette images
#define GENERATE_WRITE_TILE_BIT_ALIGNED_PALETTE(z, n, data)\
    BOOST_AUTO_TEST_CASE( BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(write_palette_tile_and_compare_with_,data),_strip_),n), bit) )\
    { \
      using namespace std; \
      using namespace boost; \
      using namespace gil; \
      string filename( string( "tiger-" ) + BOOST_PP_STRINGIZE(data) + "-strip-" ); \
      string path( tiff_in_GM ); \
      string padding(""); \
      if(BOOST_PP_LESS(n, 10)==1) \
        padding = "0"; \
      filename = filename + padding + BOOST_PP_STRINGIZE(n) + ".tif"; \
      path += filename; \
      rgb16_image_t img_strip, img_saved; \
      read_image( path, img_strip, tag_t() ); \
      image_write_info<tag_t> info; \
      info._is_tiled = true; \
      info._tile_width = info._tile_length = 16; \
    } \

#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#endif // BOOST_GIL_TIFF_TILED_READ_MACROS_HPP
