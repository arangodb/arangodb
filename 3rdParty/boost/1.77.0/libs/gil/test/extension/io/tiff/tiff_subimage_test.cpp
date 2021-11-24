//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff.hpp>

#include <boost/core/lightweight_test.hpp>

#include <fstream>
#include <sstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

namespace gil = boost::gil;

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/comparison/less.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#define BOOST_GIL_TEST_NAME_SUBIMAGE_TEST(n, data) \
    BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(test_, BOOST_PP_CAT(data, _)), n), bit_bit_aligned)

#define BOOST_GIL_TEST_GENERATE_SUBIMAGE_TEST(z, n, data)                                          \
    void BOOST_GIL_TEST_NAME_SUBIMAGE_TEST(n, data) ()   \
    {                                                                                              \
        std::string filename_strip(tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(data) + "-strip-");  \
        std::string filename_tile(tiff_in_GM + "tiger-" + BOOST_PP_STRINGIZE(data) + "-tile-");    \
        std::string padding("");                                                                   \
        if (BOOST_PP_LESS(n, 10) == 1)                                                             \
            padding = "0";                                                                         \
        filename_strip = filename_strip + padding + BOOST_PP_STRINGIZE(n) + ".tif";                \
        filename_tile  = filename_tile + padding + BOOST_PP_STRINGIZE(n) + ".tif";                 \
        gil::bit_aligned_image1_type<n, gil::gray_layout_t>::type img1, img2, img3;                \
        gil::point_t top_left(10, 10);                                                             \
        gil::point_t dim(32, 32);                                                                  \
        gil::image_read_settings<gil::tiff_tag> settings(top_left, dim);                           \
        gil::read_image(filename_strip, img1, settings);                                           \
        gil::read_image(filename_tile, img2, settings);                                            \
        gil::read_image(filename_strip, img3, gil::tiff_tag());                                    \
        BOOST_TEST(gil::equal_pixels(gil::const_view(img1), gil::const_view(img2)));               \
        BOOST_TEST(gil::equal_pixels(                                                              \
            gil::const_view(img1), gil::subimage_view(gil::view(img3), top_left, dim)));           \
    }

#define BOOST_GIL_TEST_CALL_SUBIMAGE_TEST(z, n, data) \
    BOOST_GIL_TEST_NAME_SUBIMAGE_TEST(n, data);

BOOST_PP_REPEAT_FROM_TO(1, 8, BOOST_GIL_TEST_GENERATE_SUBIMAGE_TEST, minisblack)
BOOST_PP_REPEAT_FROM_TO(9, 16, BOOST_GIL_TEST_GENERATE_SUBIMAGE_TEST, minisblack)
BOOST_PP_REPEAT_FROM_TO(17, 27, BOOST_GIL_TEST_GENERATE_SUBIMAGE_TEST, minisblack)
// TODO: there is a bug somewhere when the number of bits is 27 up to 31.

void test_subimage_test_8()
{
    gil::gray8_image_t img1, img2, img3;
    gil::point_t top_left(10, 10);
    gil::point_t dim(32, 32);

    gil::image_read_settings<gil::tiff_tag> settings(top_left, dim);

    gil::read_image(tiff_in_GM + "tiger-minisblack-strip-08.tif", img1, settings);
    gil::read_image(tiff_in_GM + "tiger-minisblack-tile-08.tif", img2, settings);
    gil::read_image(tiff_in_GM + "tiger-minisblack-strip-08.tif", img3, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img1), gil::const_view(img2)));
    BOOST_TEST(gil::equal_pixels(gil::const_view(img1), gil::subimage_view(gil::view(img3), top_left, dim)));
}

void test_subimage_test_16()
{
    gil::gray16_image_t img1, img2, img3;
    gil::point_t top_left(10, 10);
    gil::point_t dim(32, 32);

    gil::image_read_settings<gil::tiff_tag> settings(top_left, dim);

    gil::read_image(tiff_in_GM + "tiger-minisblack-strip-16.tif", img1, settings);
    gil::read_image(tiff_in_GM + "tiger-minisblack-tile-16.tif", img2, settings);
    gil::read_image(tiff_in_GM + "tiger-minisblack-strip-16.tif", img3, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img1), gil::const_view(img2)));
    BOOST_TEST(gil::equal_pixels(gil::const_view(img1), gil::subimage_view(gil::view(img3), top_left, dim)));
}

void test_subimage_test_32()
{
    using gray32_pixel_t = gil::pixel<unsigned int, gil::gray_layout_t>;
    gil::image<gray32_pixel_t, false> img1, img2, img3;

    gil::point_t top_left(10, 10);
    gil::point_t dim(32, 32);

    gil::image_read_settings<gil::tiff_tag> settings(top_left, dim);

    gil::read_image(tiff_in_GM + "tiger-minisblack-strip-32.tif", img1, settings);
    gil::read_image(tiff_in_GM + "tiger-minisblack-tile-32.tif", img2, settings);
    gil::read_image(tiff_in_GM + "tiger-minisblack-strip-32.tif", img3, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img1), gil::const_view(img2)));
    BOOST_TEST(gil::equal_pixels(gil::const_view(img1), gil::subimage_view(gil::view(img3), top_left, dim)));
}

int main()
{
    test_subimage_test_8();
    test_subimage_test_16();
    test_subimage_test_32();

    BOOST_PP_REPEAT_FROM_TO(1, 8, BOOST_GIL_TEST_CALL_SUBIMAGE_TEST, minisblack)
    BOOST_PP_REPEAT_FROM_TO(9, 16, BOOST_GIL_TEST_CALL_SUBIMAGE_TEST, minisblack)
    BOOST_PP_REPEAT_FROM_TO(17, 27, BOOST_GIL_TEST_CALL_SUBIMAGE_TEST, minisblack)

    return boost::report_errors();
}

#else
int main() {}
#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES
