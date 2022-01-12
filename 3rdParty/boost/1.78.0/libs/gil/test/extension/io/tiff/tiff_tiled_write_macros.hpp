//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_EXTENSION_IO_TIFF_TIFF_TILED_WRITE_MACROS_HPP
#define BOOST_GIL_TEST_EXTENSION_IO_TIFF_TIFF_TILED_WRITE_MACROS_HPP

#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff.hpp>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/comparison/less.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#include <string>

#include "paths.hpp"

// TODO: Rename macros to use BOOST_GIL_ prefix. See https://github.com/boostorg/gil/issues/410 ~mloskot
// TODO: Make sure generated test cases are executed. See tiff_subimage_test.cpp. ~mloskot

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#define GENERATE_WRITE_TILE_BIT_ALIGNED_RGB(z, n, data)                                            \
    void BOOST_PP_CAT(                                                                             \
        BOOST_PP_CAT(                                                                              \
            BOOST_PP_CAT(BOOST_PP_CAT(write_rgb_tile_and_compare_with_, data), _strip_), n),       \
        bit_bit_aligned)()                                                                         \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename(std::string("tiger-") + BOOST_PP_STRINGIZE(data) + "-strip-contig-");           \
        std::string path(tiff_in_GM);                                                                   \
        std::string padding("");                                                                        \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename += padding + BOOST_PP_STRINGIZE(n) + ".tif";                                      \
        path += filename;                                                                          \
        gil::bit_aligned_image3_type<n, n, n, gil::rgb_layout_t>::type img_strip, img_saved;       \
        gil::read_image(path, img_strip, gil::tiff_tag());                                         \
        gil::image_write_info<gil::tiff_tag> info;                                                 \
        info._is_tiled   = true;                                                                   \
        info._tile_width = info._tile_length = 16;                                                 \
        gil::write_view(tiff_out + filename, gil::view(img_strip), info);                          \
        gil::read_image(tiff_out + filename, img_saved, gil::tiff_tag());                          \
        BOOST_TEST_EQ(                                                                             \
            gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_saved)), true);      \
    }

// Special case for minisblack images
#define GENERATE_WRITE_TILE_BIT_ALIGNED_MINISBLACK(z, n, data)                                     \
    void BOOST_PP_CAT(                                                                             \
        BOOST_PP_CAT(                                                                              \
            BOOST_PP_CAT(BOOST_PP_CAT(write_minisblack_tile_and_compare_with_, data), _strip_),    \
            n),                                                                                    \
        bit_bit_aligned)()                                                                         \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename(std::string("tiger-") + BOOST_PP_STRINGIZE(data) + "-strip-");                  \
        std::string path(tiff_in_GM);                                                                   \
        std::string padding("");                                                                        \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename = filename + padding + BOOST_PP_STRINGIZE(n) + ".tif";                            \
        path += filename;                                                                          \
        gil::bit_aligned_image1_type<n, gil::gray_layout_t>::type img_strip, img_saved;                 \
        gil::read_image(path, img_strip, gil::tiff_tag());                                         \
        gil::image_write_info<gil::tiff_tag> info;                                                 \
        info._is_tiled   = true;                                                                   \
        info._tile_width = info._tile_length = 16;                                                 \
        gil::write_view(tiff_out + filename, gil::view(img_strip), info);                          \
        gil::read_image(tiff_out + filename, img_saved, gil::tiff_tag());                          \
        BOOST_TEST_EQ(                                                                             \
            gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_saved)), true);      \
    }

// Special case for palette images
#define GENERATE_WRITE_TILE_BIT_ALIGNED_PALETTE(z, n, data)                                        \
    void BOOST_PP_CAT(                                                                             \
        BOOST_PP_CAT(                                                                              \
            BOOST_PP_CAT(BOOST_PP_CAT(write_palette_tile_and_compare_with_, data), _strip_), n),   \
        bit)()                                                                                     \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename(std::string("tiger-") + BOOST_PP_STRINGIZE(data) + "-strip-");                  \
        std::string path(tiff_in_GM);                                                                   \
        std::string padding("");                                                                        \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename = filename + padding + BOOST_PP_STRINGIZE(n) + ".tif";                            \
        path += filename;                                                                          \
        gil::rgb16_image_t img_strip, img_saved;                                                   \
        gil::read_image(path, img_strip, gil::tiff_tag());                                         \
        gil::image_write_info<gil::tiff_tag> info;                                                 \
        info._is_tiled   = true;                                                                   \
        info._tile_width = info._tile_length = 16;                                                 \
        gil::write_view(tiff_out + filename, gil::view(img_strip), info);                          \
        gil::read_image(tiff_out + filename, img_saved, gil::tiff_tag());                          \
        BOOST_TEST_EQ(                                                                             \
            gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_saved)), true);      \
    }

#else

#define GENERATE_WRITE_TILE_BIT_ALIGNED_RGB(z, n, data)                                            \
    void BOOST_PP_CAT(                                                                             \
        BOOST_PP_CAT(                                                                              \
            BOOST_PP_CAT(BOOST_PP_CAT(write_rgb_tile_and_compare_with_, data), _strip_), n),       \
        bit_bit_aligned)()                                                                         \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename(std::string("tiger-") + BOOST_PP_STRINGIZE(data) + "-strip-contig-");           \
        std::string path(tiff_in_GM);                                                                   \
        std::string padding("");                                                                        \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename += padding + BOOST_PP_STRINGIZE(n) + ".tif";                                      \
        path += filename;                                                                          \
        gil::bit_aligned_image3_type<n, n, n, gil::rgb_layout_t>::type img_strip, img_saved;       \
        gil::read_image(path, img_strip, gil::tiff_tag());                                         \
        gil::image_write_info<gil::tiff_tag> info;                                                 \
        info._is_tiled   = true;                                                                   \
        info._tile_width = info._tile_length = 16;                                                 \
    }

// Special case for minisblack images
#define GENERATE_WRITE_TILE_BIT_ALIGNED_MINISBLACK(z, n, data)                                     \
    voidBOOST_PP_CAT(                                                                              \
        BOOST_PP_CAT(                                                                              \
            BOOST_PP_CAT(BOOST_PP_CAT(write_minisblack_tile_and_compare_with_, data), _strip_),    \
            n),                                                                                    \
        bit_bit_aligned)()                                                                         \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename(std::string("tiger-") + BOOST_PP_STRINGIZE(data) + "-strip-");                  \
        std::string path(tiff_in_GM);                                                                   \
        std::string padding("");                                                                        \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename = filename + padding + BOOST_PP_STRINGIZE(n) + ".tif";                            \
        path += filename;                                                                          \
        gil::bit_aligned_image1_type<n, gil::gray_layout_t>::type img_strip, img_saved;            \
        gil::read_image(path, img_strip, gil::tiff_tag());                                         \
        gil::image_write_info<gil::tiff_tag> info;                                                 \
        info._is_tiled   = true;                                                                   \
        info._tile_width = info._tile_length = 16;                                                 \
    }

// Special case for palette images
#define GENERATE_WRITE_TILE_BIT_ALIGNED_PALETTE(z, n, data)                                        \
    void BOOST_PP_CAT(                                                                             \
        BOOST_PP_CAT(                                                                              \
            BOOST_PP_CAT(BOOST_PP_CAT(write_palette_tile_and_compare_with_, data), _strip_), n),   \
        bit)()                                                                                     \
    {                                                                                              \
        namespace gil = boost::gil;                                                                \
        std::string filename(std::string("tiger-") + BOOST_PP_STRINGIZE(data) + "-strip-");                  \
        std::string path(tiff_in_GM);                                                                   \
        std::string padding("");                                                                        \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename = filename + padding + BOOST_PP_STRINGIZE(n) + ".tif";                            \
        path += filename;                                                                          \
        gil::rgb16_image_t img_strip, img_saved;                                                   \
        gil::read_image(path, img_strip, gil::tiff_tag());                                         \
        gil::image_write_info<gil::tiff_tag> info;                                                 \
        info._is_tiled   = true;                                                                   \
        info._tile_width = info._tile_length = 16;                                                 \
    }

#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#endif // BOOST_GIL_TEST_EXTENSION_IO_TIFF_TIFF_TILED_WRITE_MACROS_HPP
