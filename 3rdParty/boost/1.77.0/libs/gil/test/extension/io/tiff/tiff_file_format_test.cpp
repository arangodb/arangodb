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

#include <string>

#include "paths.hpp"
#include "scanline_read_test.hpp"

namespace gil = boost::gil;

#ifdef BOOST_GIL_IO_USE_TIFF_LIBTIFF_TEST_SUITE_IMAGES
template <typename Image>
void test_tiff_scanline_reader(std::string filename)
{
    test_scanline_reader<Image, gil::tiff_tag>(filename.c_str());
}

// 73x43 2-bit minisblack gray image
void test_two_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-02.tif");

    using image_t = gil::bit_aligned_image1_type<2, gil::gray_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test4.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-04.tif    73x43 4-bit minisblack gray image
void test_four_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-04.tif");

    using image_t = gil::bit_aligned_image1_type<4, gil::gray_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test5.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-06.tif 73x43 6-bit minisblack gray image
void test_six_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-06.tif");

    using image_t = gil::bit_aligned_image1_type<6, gil::gray_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test6.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-08.tif    73x43 8-bit minisblack gray image
void test_eight_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-08.tif");

    using image_t = gray8_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test7.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-10.tif    73x43 10-bit minisblack gray image
void test_ten_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-10.tif");

    using image_t = gil::bit_aligned_image1_type<10, gil::gray_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test8.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-12.tif    73x43 12-bit minisblack gray image
void test_twelve_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-12.tif");

        using image_t = gil::bit_aligned_image1_type<12, gil::gray_layout_t>::type;
        image_t img;
        gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        gil::write_view(tiff_out + "test9.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-14.tif    73x43 14-bit minisblack gray image
void test_fourteen_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-14.tif");

    using image_t = gil::bit_aligned_image1_type<14, gil::gray_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test10.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-16.tif    73x43 16-bit minisblack gray image
void test_sixteen_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-16.tif");

    using image_t = gray16_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test11.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-24.tif    73x43 24-bit minisblack gray image
void test_twentyfour_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-24.tif");

    using image_t = gil::bit_aligned_image1_type<24, gil::gray_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test12.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-minisblack-32.tif    73x43 32-bit minisblack gray image
void test_thirtytwo_bit_minisblack_gray_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-minisblack-32.tif");

    using image_t = gray32_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test13.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-palette-02.tif 73x43 4-entry colormapped image
void test_four_entry_colormapped_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-palette-02.tif");

    using image_t = gil::rgb16_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test14.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-palette-04.tif    73x43 16-entry colormapped image
void test_sixteen_entry_colormapped_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-palette-04.tif");

    using image_t = gil::rgb16_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test15.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-palette-08.tif    73x43 256-entry colormapped image
void test_twohundred_twenty_five_entry_colormapped_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-palette-08.tif");

    using image_t = gil::rgb16_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test16.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-palette-16.tif    73x43 65536-entry colormapped image
void test_sixtyfive_thousand_entry_colormapped_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-palette-16.tif");

    using image_t = gil::rgb16_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test17.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-02.tif    73x43 2-bit contiguous RGB image
void test_two_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-02.tif");

    using image_t = gil::bit_aligned_image3_type<2, 2, 2, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test18.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-04.tif    73x43 4-bit contiguous RGB image
void test_four_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-04.tif");

    using image_t = gil::bit_aligned_image3_type<4, 4, 4, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test19.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-08.tif    73x43 8-bit contiguous RGB image
void test_eight_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-08.tif");

    using image_t = gil::rgb8_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test20.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-10.tif    73x43 10-bit contiguous RGB image
void test_ten_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-10.tif");

    using image_t = gil::bit_aligned_image3_type<10, 10, 10, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test21.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-12.tif    73x43 12-bit contiguous RGB image
void test_twelve_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-12.tif");

    using image_t = gil::bit_aligned_image3_type<12, 12, 12, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test22.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-14.tif    73x43 14-bit contiguous RGB image
void test_fourteen_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-14.tif");

    using image_t = gil::bit_aligned_image3_type<14, 14, 14, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test23.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-16.tif    73x43 16-bit contiguous RGB image
void test_sixteen_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-16.tif");

    using image_t = gil::rgb16_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test24.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-24.tif    73x43 24-bit contiguous RGB image
void test_twenty_four_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-24.tif");

    using image_t = gil::bit_aligned_image3_type<24, 24, 24, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test25.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-contig-32.tif    73x43 32-bit contiguous RGB image
void test_thirty_two_bit_contiguous_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-contig-32.tif");

    using image_t = gil::rgb32_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test26.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-02.tif    73x43 2-bit seperated RGB image
void test_two_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-02.tif");

    using image_t = gil::bit_aligned_image3_type<2, 2, 2, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test27.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-04.tif    73x43 4-bit seperated RGB image
void test_four_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-04.tif");

    using image_t = gil::bit_aligned_image3_type<4, 4, 4, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test28.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-08.tif    73x43 8-bit seperated RGB image
void test_eight_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-08.tif");

    using image_t = gil::rgb8_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test29.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-10.tif    73x43 10-bit seperated RGB image
void test_ten_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-10.tif");

    using image_t = gil::bit_aligned_image3_type<10, 10, 10, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test30.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-12.tif    73x43 12-bit seperated RGB image
void test_twelve_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-12.tif");

    using image_t = gil::bit_aligned_image3_type<12, 12, 12, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test31.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-14.tif    73x43 14-bit seperated RGB image
void test_fourteen_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-14.tif");

    using image_t = gil::bit_aligned_image3_type<14, 14, 14, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test32.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-16.tif    73x43 16-bit seperated RGB image
void test_sixteen_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-16.tif");

    using image_t = gil::rgb16_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test33.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-24.tif    73x43 24-bit seperated RGB image
void test_twenty_four_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-24.tif");

    using image_t = gil::bit_aligned_image3_type<24, 24, 24, gil::rgb_layout_t>::type;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test34.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-rgb-planar-32.tif    73x43 32-bit seperated RGB image
void test_thirty_two_bit_seperated_rgb_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-rgb-planar-32.tif");

    using image_t = gil::rgb32_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test35.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-separated-contig-08.tif    73x43 8-bit contiguous CMYK image
void test_eight_bit_contiguous_cmyk_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-separated-contig-08.tif");

    using image_t = gil::cmyk8_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test36.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-separated-contig-16.tif    73x43 16-bit contiguous CMYK image
void test_sixteen_bit_contiguous_cmyk_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-separated-contig-16.tif");

    using image_t = gil::cmyk16_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test37.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-separated-planar-08.tif    73x43 8-bit separated CMYK image
void test_eight_bit_separated_cmyk_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-separated-planar-08.tif");

    using image_t = gil::cmyk8_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test38.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

// flower-separated-planar-16.tif    73x43 16-bit separated CMYK image
void test_sixteen_bit_separated_cmyk_image()
{
    std::string filename(tiff_in + "libtiffpic/depth/flower-separated-planar-16.tif");

    using image_t = gil::cmyk16_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test39.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}
#endif  // BOOST_GIL_IO_USE_TIFF_LIBTIFF_TEST_SUITE_IMAGES

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES
void test_tiger_separated_strip_contig_08()
{
    std::string filename(tiff_in_GM + "tiger-separated-strip-contig-08.tif");

    using image_t = gil::cmyk8_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test40.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_tiger_separated_strip_contig_16()
{
    std::string filename(tiff_in_GM + "tiger-separated-strip-contig-16.tif");

    using image_t = gil::cmyk16_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test41.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_tiger_separated_strip_planar_08()
{
    std::string filename(tiff_in_GM + "tiger-separated-strip-planar-08.tif");

    using image_t = gil::cmyk8_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test42.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_tiger_separated_strip_planar_16()
{
    std::string filename(tiff_in_GM + "tiger-separated-strip-planar-16.tif");

    using image_t = gil::cmyk16_planar_image_t;
    image_t img;
    gil::read_image(filename, img, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "test43.tif", gil::view(img), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES    }
}
#endif  // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

int main()
{
#ifdef BOOST_GIL_IO_USE_TIFF_LIBTIFF_TEST_SUITE_IMAGES
    test_two_bit_minisblack_gray_image();
    test_four_bit_minisblack_gray_image();
    test_six_bit_minisblack_gray_image();
    test_eight_bit_minisblack_gray_image();
    test_ten_bit_minisblack_gray_image();
    test_twelve_bit_minisblack_gray_image();
    test_fourteen_bit_minisblack_gray_image();
    test_sixteen_bit_minisblack_gray_image();
    test_twentyfour_bit_minisblack_gray_image();
    test_thirtytwo_bit_minisblack_gray_image();
    test_four_entry_colormapped_image();
    test_sixteen_entry_colormapped_image();
    test_twohundred_twenty_five_entry_colormapped_image();
    test_sixtyfive_thousand_entry_colormapped_image();
    test_two_bit_contiguous_rgb_image();
    test_four_bit_contiguous_rgb_image();
    test_eight_bit_contiguous_rgb_image();
    test_ten_bit_contiguous_rgb_image();
    test_twelve_bit_contiguous_rgb_image();
    test_fourteen_bit_contiguous_rgb_image();
    test_sixteen_bit_contiguous_rgb_image();
    test_twenty_four_bit_contiguous_rgb_image();
    test_thirty_two_bit_contiguous_rgb_image();
    test_two_bit_seperated_rgb_image();
    test_four_bit_seperated_rgb_image();
    test_eight_bit_seperated_rgb_image();
    test_ten_bit_seperated_rgb_image();
    test_twelve_bit_seperated_rgb_image();
    test_fourteen_bit_seperated_rgb_image();
    test_sixteen_bit_seperated_rgb_image();
    test_twenty_four_bit_seperated_rgb_image();
    test_thirty_two_bit_seperated_rgb_image();
    test_eight_bit_contiguous_cmyk_image();
    test_sixteen_bit_contiguous_cmyk_image();
    test_eight_bit_separated_cmyk_image();
    test_sixteen_bit_separated_cmyk_image();
#endif

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES
    test_tiger_separated_strip_contig_08();
    test_tiger_separated_strip_contig_16();
    test_tiger_separated_strip_planar_08();
    test_tiger_separated_strip_planar_16();
#endif

#if defined(BOOST_GIL_IO_USE_TIFF_LIBTIFF_TEST_SUITE_IMAGES) || defined(BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES)
    return boost::report_errors();
#endif
}
