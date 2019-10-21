//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE tiff_file_format_test_module

#include <boost/gil/extension/io/tiff.hpp>

#include <boost/test/unit_test.hpp>

#include "paths.hpp"
#include "scanline_read_test.hpp"

using namespace std;
using namespace boost::gil;

using tag_t = tiff_tag;

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_LIBTIFF_TEST_SUITE_IMAGES

template< typename Image >
void test_tiff_scanline_reader( string filename )
{
    test_scanline_reader<Image, tag_t>( filename.c_str() );
}

// 73x43 2-bit minisblack gray image
BOOST_AUTO_TEST_CASE( two_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-02.tif" );

    {
        using image_t = bit_aligned_image1_type<2, gray_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test4.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-04.tif    73x43 4-bit minisblack gray image
BOOST_AUTO_TEST_CASE( four_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-04.tif" );

    {
        using image_t = bit_aligned_image1_type<4, gray_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test5.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-06.tif 73x43 6-bit minisblack gray image
BOOST_AUTO_TEST_CASE( six_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-06.tif" );

    {
        using image_t = bit_aligned_image1_type<6, gray_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test6.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-08.tif    73x43 8-bit minisblack gray image
BOOST_AUTO_TEST_CASE( eight_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-08.tif" );

    {
        using image_t = gray8_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test7.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-10.tif    73x43 10-bit minisblack gray image
BOOST_AUTO_TEST_CASE( ten_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-10.tif" );

    {
        using image_t = bit_aligned_image1_type<10, gray_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test8.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-12.tif    73x43 12-bit minisblack gray image
BOOST_AUTO_TEST_CASE( twelve_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-12.tif" );

    {
        using image_t = bit_aligned_image1_type<12, gray_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test9.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-14.tif    73x43 14-bit minisblack gray image
BOOST_AUTO_TEST_CASE( fourteen_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-14.tif" );

    {
        using image_t = bit_aligned_image1_type<14, gray_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test10.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-16.tif    73x43 16-bit minisblack gray image
BOOST_AUTO_TEST_CASE( sixteen_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-16.tif" );

    {
        using image_t = gray16_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test11.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-24.tif    73x43 24-bit minisblack gray image
BOOST_AUTO_TEST_CASE( twentyfour_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-24.tif" );

    {
        using image_t = bit_aligned_image1_type<24, gray_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test12.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-minisblack-32.tif    73x43 32-bit minisblack gray image
BOOST_AUTO_TEST_CASE( thirtytwo_bit_minisblack_gray_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-minisblack-32.tif" );

    {
        using image_t = gray32_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test13.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-palette-02.tif 73x43 4-entry colormapped image
BOOST_AUTO_TEST_CASE( four_entry_colormapped_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-palette-02.tif" );

    {
        using image_t = rgb16_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test14.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-palette-04.tif    73x43 16-entry colormapped image
BOOST_AUTO_TEST_CASE( sixteen_entry_colormapped_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-palette-04.tif" );

    {
        using image_t = rgb16_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test15.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-palette-08.tif    73x43 256-entry colormapped image
BOOST_AUTO_TEST_CASE( twohundred_twenty_five_entry_colormapped_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-palette-08.tif" );

    {
        using image_t = rgb16_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test16.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-palette-16.tif    73x43 65536-entry colormapped image
BOOST_AUTO_TEST_CASE( sixtyfive_thousand_entry_colormapped_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-palette-16.tif" );

    {
        using image_t = rgb16_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test17.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-02.tif    73x43 2-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( two_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-02.tif" );

    {
        using image_t = bit_aligned_image3_type<2, 2, 2, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test18.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-04.tif    73x43 4-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( four_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-04.tif" );

    {
        using image_t = bit_aligned_image3_type<4, 4, 4, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test19.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-08.tif    73x43 8-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( eight_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-08.tif" );

    {
        using image_t = rgb8_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test20.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-10.tif    73x43 10-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( ten_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-10.tif" );

    {
        using image_t = bit_aligned_image3_type<10, 10, 10, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test21.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-12.tif    73x43 12-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( twelve_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-12.tif" );

    {
        using image_t = bit_aligned_image3_type<12, 12, 12, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test22.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-14.tif    73x43 14-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( fourteen_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-14.tif" );

    {
        using image_t = bit_aligned_image3_type<14, 14, 14, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test23.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-16.tif    73x43 16-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( sixteen_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-16.tif" );

    {
        using image_t = rgb16_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test24.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-24.tif    73x43 24-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( twenty_four_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-24.tif" );

    {
        using image_t = bit_aligned_image3_type<24, 24, 24, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test25.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-contig-32.tif    73x43 32-bit contiguous RGB image
BOOST_AUTO_TEST_CASE( thirty_two_bit_contiguous_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-contig-32.tif" );

    {
        using image_t = rgb32_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test26.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-02.tif    73x43 2-bit seperated RGB image
BOOST_AUTO_TEST_CASE( two_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-02.tif" );

    {
        using image_t = bit_aligned_image3_type<2, 2, 2, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test27.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-04.tif    73x43 4-bit seperated RGB image
BOOST_AUTO_TEST_CASE( four_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-04.tif" );

    {
        using image_t = bit_aligned_image3_type<4, 4, 4, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test28.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-08.tif    73x43 8-bit seperated RGB image
BOOST_AUTO_TEST_CASE( eight_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-08.tif" );

    {
        using image_t = rgb8_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test29.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-10.tif    73x43 10-bit seperated RGB image
BOOST_AUTO_TEST_CASE( ten_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-10.tif" );

    {
        using image_t = bit_aligned_image3_type<10, 10, 10, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test30.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-12.tif    73x43 12-bit seperated RGB image
BOOST_AUTO_TEST_CASE( twelve_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-12.tif" );

    {
        using image_t = bit_aligned_image3_type<12, 12, 12, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test31.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-14.tif    73x43 14-bit seperated RGB image
BOOST_AUTO_TEST_CASE( fourteen_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-14.tif" );

    {
        using image_t = bit_aligned_image3_type<14, 14, 14, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test32.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-16.tif    73x43 16-bit seperated RGB image
BOOST_AUTO_TEST_CASE( sixteen_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-16.tif" );

    {
        using image_t = rgb16_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test33.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-24.tif    73x43 24-bit seperated RGB image
BOOST_AUTO_TEST_CASE( twenty_four_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-24.tif" );

    {
        using image_t = bit_aligned_image3_type<24, 24, 24, rgb_layout_t>::type;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test34.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-rgb-planar-32.tif    73x43 32-bit seperated RGB image
BOOST_AUTO_TEST_CASE( thirty_two_bit_seperated_RGB_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-rgb-planar-32.tif" );

    {
        using image_t = rgb32_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test35.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-separated-contig-08.tif    73x43 8-bit contiguous CMYK image
BOOST_AUTO_TEST_CASE( eight_bit_contiguous_CMYK_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-separated-contig-08.tif" );

    {
        using image_t = cmyk8_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test36.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-separated-contig-16.tif    73x43 16-bit contiguous CMYK image
BOOST_AUTO_TEST_CASE( sixteen_bit_contiguous_CMYK_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-separated-contig-16.tif" );

    {
        using image_t = cmyk16_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test37.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-separated-planar-08.tif    73x43 8-bit separated CMYK image
BOOST_AUTO_TEST_CASE( eight_bit_separated_CMYK_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-separated-planar-08.tif" );

    {
        using image_t = cmyk8_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test38.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

// flower-separated-planar-16.tif    73x43 16-bit separated CMYK image
BOOST_AUTO_TEST_CASE( sixteen_bit_separated_CMYK_image_test )
{
    std::string filename( tiff_in + "libtiffpic/depth/flower-separated-planar-16.tif" );

    {
        using image_t = cmyk16_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test39.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

#endif // BOOST_GIL_IO_USE_TIFF_LIBTIFF_TEST_SUITE_IMAGES

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_CASE( tiger_separated_strip_contig_08 )
{
    std::string filename( tiff_in_GM + "tiger-separated-strip-contig-08.tif" );

    {
        using image_t = cmyk8_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test40.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

BOOST_AUTO_TEST_CASE( tiger_separated_strip_contig_16 )
{
    std::string filename( tiff_in_GM + "tiger-separated-strip-contig-16.tif" );

    {
        using image_t = cmyk16_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test41.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

BOOST_AUTO_TEST_CASE( tiger_separated_strip_planar_08 )
{
    std::string filename( tiff_in_GM + "tiger-separated-strip-planar-08.tif" );

    {
        using image_t = cmyk8_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test42.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

BOOST_AUTO_TEST_CASE( tiger_separated_strip_planar_16 )
{
    std::string filename( tiff_in_GM + "tiger-separated-strip-planar-16.tif" );

    {
        using image_t = cmyk16_planar_image_t;
        image_t img;

        read_image( filename
                  , img
                  , tag_t()
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( tiff_out + "test43.tif"
                  , view( img )
                  , tiff_tag()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
