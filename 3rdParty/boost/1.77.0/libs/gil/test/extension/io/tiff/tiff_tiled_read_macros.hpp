//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_EXTENSION_IO_TIFF_TIFF_TILED_READ_MACROS_HPP
#define BOOST_GIL_TEST_EXTENSION_IO_TIFF_TIFF_TILED_READ_MACROS_HPP

#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff/read.hpp>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/comparison/less.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>

#include "paths.hpp"

// TODO: Rename macros to use BOOST_GIL_ prefix. See https://github.com/boostorg/gil/issues/410 ~mloskot
// TODO: Make sure generated test cases are executed. See tiff_subimage_test.cpp. ~mloskot

#define BOOST_GIL_TEST_NAME_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB(n, data)                     \
    BOOST_PP_CAT(                                                                                  \
        BOOST_PP_CAT(                                                                              \
            BOOST_PP_CAT(                                                                          \
                BOOST_PP_CAT(                                                                      \
                    BOOST_PP_CAT(read_tile_and_compare_with_, BOOST_PP_TUPLE_ELEM(2, 0, data)),    \
                    BOOST_PP_TUPLE_ELEM(2, 1, data)),                                              \
                _strip_),                                                                          \
            n),                                                                                    \
        bit_bit_aligned)

#define BOOST_GIL_TEST_GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB(z, n, data)                  \
    void BOOST_GIL_TEST_NAME_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB(n, data)()                  \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename_strip(                                                                \
            tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 0, data)) +          \
            "-strip-" + BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, data)) + "-");                \
        std::string filename_tile(                                                                 \
            tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 0, data)) +          \
            "-tile-" + BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, data)) + "-");                 \
        std::string padding("");                                                                   \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename_strip = filename_strip + padding + BOOST_PP_STRINGIZE(n) + ".tif";                \
        filename_tile  = filename_tile + padding + BOOST_PP_STRINGIZE(n) + ".tif";                 \
        gil::bit_aligned_image3_type<n, n, n, gil::rgb_layout_t>::type img_strip, img_tile;        \
        gil::read_image(filename_strip, img_strip, gil::tiff_tag());                               \
        gil::read_image(filename_tile, img_tile, gil::tiff_tag());                                 \
        BOOST_TEST_EQ(                                                                             \
            gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)), true);       \
    }

// Special case for minisblack images
#define BOOST_GIL_TEST_NAME_TILE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK(n, data)                         \
    BOOST_PP_CAT(                                                                                  \
        BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(read_tile_and_compare_with_, data), _strip_), n),   \
        bit_bit_aligned)

#define BOOST_GIL_TEST_GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK(z, n, data)           \
    void BOOST_GIL_TEST_NAME_TILE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK(n, data)()                      \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename_strip(tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(data) + "-strip-");  \
        std::string filename_tile(tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(data) + "-tile-");    \
        std::string padding("");                                                                   \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename_strip = filename_strip + padding + BOOST_PP_STRINGIZE(n) + ".tif";                \
        filename_tile  = filename_tile + padding + BOOST_PP_STRINGIZE(n) + ".tif";                 \
        gil::bit_aligned_image1_type<n, gil::gray_layout_t>::type img_strip, img_tile;             \
        gil::read_image(filename_strip, img_strip, gil::tiff_tag());                               \
        gil::read_image(filename_tile, img_tile, gil::tiff_tag());                                 \
        BOOST_TEST_EQ(                                                                             \
            gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)), true);       \
    }

// Special case for palette images
#define BOOST_GIL_TEST_NAME_TILE_STRIP_COMPARISON_PALETTE(n, data)                                 \
    BOOST_PP_CAT(                                                                                  \
        BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(read_tile_and_compare_with_, data), _strip_), n),   \
        bit)

#define BOOST_GIL_TEST_GENERATE_TILE_STRIP_COMPARISON_PALETTE(z, n, data)                          \
    void BOOST_GIL_TEST_NAME_TILE_STRIP_COMPARISON_PALETTE(n, data)()                              \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename_strip(tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(data) + "-strip-");  \
        std::string filename_tile(tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(data) + "-tile-");    \
        std::string padding("");                                                                   \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename_strip = filename_strip + padding + BOOST_PP_STRINGIZE(n) + ".tif";                \
        filename_tile  = filename_tile + padding + BOOST_PP_STRINGIZE(n) + ".tif";                 \
        gil::rgb16_image_t img_strip, img_tile;                                                    \
        gil::read_image(filename_strip, img_strip, gil::tiff_tag());                               \
        gil::read_image(filename_tile, img_tile, gil::tiff_tag());                                 \
        BOOST_TEST_EQ(                                                                             \
            gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)), true);       \
    }

#define BOOST_GIL_TEST_CALL_TILE_STRIP_COMPARISON_PALETTE(z, n, data) \
    BOOST_GIL_TEST_NAME_TILE_STRIP_COMPARISON_PALETTE(n, data);


#endif // BOOST_GIL_TEST_EXTENSION_IO_TIFF_TIFF_TILED_READ_MACROS_HPP
