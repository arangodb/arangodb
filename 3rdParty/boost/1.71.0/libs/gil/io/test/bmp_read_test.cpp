//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE bmp_read_test_module
#include <boost/gil.hpp>
#include <boost/gil/extension/io/bmp.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/type_traits/is_same.hpp>

#include "paths.hpp"
#include "scanline_read_test.hpp"

using namespace std;
using namespace boost::gil;

using tag_t = bmp_tag;

BOOST_AUTO_TEST_SUITE( gil_io_bmp_tests )

#ifdef BOOST_GIL_IO_USE_BMP_TEST_SUITE_IMAGES

template< typename Image >
void write( Image&        img
          , const string& file_name
          )
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( bmp_out + file_name
              , view( img )
              , tag_t()
              );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( read_header_test )
{
    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( bmp_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._offset               ,      54u );
        BOOST_CHECK_EQUAL( backend._info._header_size          ,      40u );
        BOOST_CHECK_EQUAL( backend._info._width                ,     1000 );
        BOOST_CHECK_EQUAL( backend._info._height               ,      600 );
        BOOST_CHECK_EQUAL( backend._info._bits_per_pixel       ,       24 );
        BOOST_CHECK_EQUAL( backend._info._compression          ,       0u );
        BOOST_CHECK_EQUAL( backend._info._image_size           , 1800000u );
        BOOST_CHECK_EQUAL( backend._info._horizontal_resolution,        0 );
        BOOST_CHECK_EQUAL( backend._info._vertical_resolution  ,        0 );
        BOOST_CHECK_EQUAL( backend._info._num_colors           ,       0u );
        BOOST_CHECK_EQUAL( backend._info._num_important_colors ,       0u );
        BOOST_CHECK_EQUAL( backend._info._valid                ,     true );
    }
}

BOOST_AUTO_TEST_CASE( read_reference_images_test )
{
    // comments are taken from http://entropymine.com/jason/bmpsuite/reference/reference.html

    // g01bw.bmp - black and white palette (#000000,#FFFFFF)
    {
        rgba8_image_t img;
        read_image( bmp_in + "g01bw.bmp", img, tag_t() );
    }

    // g01wb.bmp - white and black palette (#FFFFFF,#000000).
    // Should look the same as g01bw, not inverted.
    {
        rgba8_image_t img;

        read_image( bmp_in + "g01wb.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g01wb.bmp" );
    }

    // g01bg.bmp - blue and green palette (#4040FF,#40FF40)
    {
        rgba8_image_t img;

        read_image( bmp_in + "g01bg.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g01bg.bmp" );
    }

    // g01p1.bmp - 1-color (blue) palette (#4040FF)
    {
        rgba8_image_t img;

        read_image( bmp_in + "g01p1.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g01p1.bmp" );
    }

    // g04.bmp - basic 4bpp (16 color) image
    {
        rgba8_image_t img;

        read_image( bmp_in + "g04.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g04.bmp" );
    }

    // g04rle.bmp - RLE compressed.
    {
        rgb8_image_t img;

        read_image( bmp_in + "g04rle.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g04rle.bmp" );
    }

    // g04p4.bmp - 4-color grayscale palette
    {
        rgba8_image_t img;

        read_image( bmp_in + "g04p4.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g04p4.bmp" );
    }

    // g08.bmp - basic 8bpp (256 color) image
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08.bmp" );
    }

    // g08p256.bmp - biClrUsed=256, biClrImportant=0 [=256]
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08p256.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08p256.bmp" );
    }

    // g08pi256.bmp - biClrUsed=256, biClrImportant=256
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08pi256.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08pi256.bmp" );
    }

    // g08pi64.bmp - biClrUsed=256, biClrImportant=64. It's barely possible that some
    // sophisticated viewers may display this image in grayscale, if there are a
    // limited number of colors available.
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08pi64.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08pi64.bmp" );
    }

    // g08rle.bmp - RLE compressed.
    {
        rgb8_image_t img;

        read_image( bmp_in + "g08rle.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08rle.bmp" );
    }

    // g08os2.bmp - OS/2-style bitmap. This is an obsolete variety of BMP
    // that is still encountered sometimes. It has 3-byte palette
    // entries (instead of 4), and 16-bit width/height fields (instead of 32).
    {
        rgb8_image_t img;

        read_image( bmp_in + "g08os2.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08os2.bmp" );
    }

    // g08res22.bmp - resolution 7874x7874 pixels/meter (200x200 dpi)
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08res22.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08res22.bmp" );
    }

    // g08res11.bmp - resolution 3937x3937 pixels/meter (100x100 dpi)
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08res11.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08res11.bmp" );
    }

    // g08res21.bmp resolution 7874x3937 pixels/meter (200x100 dpi).
    // Some programs (e.g. Imaging for Windows) may display this image
    // stretched vertically, which is the optimal thing to do if the
    // program is primarily a viewer, rather than an editor.
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08res21.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08res21.bmp" );
    }

    // g08s0.bmp - bits size not given (set to 0). This is legal for uncompressed bitmaps.
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08s0.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08s0.bmp" );
    }

    // g08offs.bmp - bfOffBits in header not set to the usual value.
    // There are 100 extra unused bytes between palette and bits.
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08offs.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08offs.bmp" );
    }

    // g08w126.bmp - size 126x63 (right and bottom slightly clipped)
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08w126.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 126u );
        BOOST_CHECK_EQUAL( view( img ).height(),  63u );

        write( img, "g08w126.bmp" );
    }

    // g08w125.bmp - size 125x62
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08w125.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 125u );
        BOOST_CHECK_EQUAL( view( img ).height(),  62u );

        write( img, "g08w125.bmp" );
    }

    // g08w124.bmp - size 124x61
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08w124.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 124u );
        BOOST_CHECK_EQUAL( view( img ).height(),  61u );

        write( img, "g08w124.bmp" );
    }

    // g08p64.bmp - 64-color grayscale palette
    {
        rgba8_image_t img;

        read_image( bmp_in + "g08p64.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g08p64.bmp" );
    }

    // g16def555.bmp - 15-bit color (1 bit wasted), biCompression=BI_RGB (no bitfields, defaults to 5-5-5)
    {
        rgb8_image_t img;

        read_image( bmp_in + "g16def555.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g16def555.bmp" );
    }

    // g16bf555.bmp - 15-bit color, biCompression=BI_BITFIELDS (bitfields indicate 5-5-5)
    {
        rgb8_image_t img;

        read_image( bmp_in + "g16bf555.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g16bf555.bmp" );
    }

    // g16bf565.bmp - 16-bit color, biCompression=BI_BITFIELDS (bitfields indicate 5-6-5)
    {
        rgb8_image_t img;

        read_image( bmp_in + "g16bf565.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g16bf565.bmp" );
    }

    // g24.bmp - 24-bit color (BGR)
    {
        rgb8_image_t img;

        read_image( bmp_in + "g24.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g24.bmp" );
    }

    // g32def.bmp - 24-bit color (8 bits wasted), biCompression=BI_RGB (no bitfields, defaults to BGRx)
    {
        rgba8_image_t img;

        read_image( bmp_in + "g32def.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g32def.bmp" );
    }

    // g32bf.bmp - 24-bit color (8 bits wasted), biCompression=BI_BITFIELDS (bitfields indicate BGRx)
    {
        rgba8_image_t img;

        read_image( bmp_in + "g32bf.bmp", img, tag_t() );
        BOOST_CHECK_EQUAL( view( img ).width() , 127u );
        BOOST_CHECK_EQUAL( view( img ).height(),  64u );

        write( img, "g32bf.bmp" );
    }
}


BOOST_AUTO_TEST_CASE( read_reference_images_image_iterator_test )
{
    // comments are taken from http://entropymine.com/jason/bmpsuite/reference/reference.html

    // g01bw.bmp - black and white palette (#000000,#FFFFFF)
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g01bw.bmp" ).c_str() );

    // g01wb.bmp - white and black palette (#FFFFFF,#000000).
    // Should look the same as g01bw, not inverted.
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g01wb.bmp" ).c_str() );

    // g01bg.bmp - blue and green palette (#4040FF,#40FF40)
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g01bg.bmp" ).c_str() );

    // g01p1.bmp - 1-color (blue) palette (#4040FF)
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g01p1.bmp" ).c_str() );

    // g04.bmp - basic 4bpp (16 color) image
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g04.bmp" ).c_str() );

    // not supported
    // g04rle.bmp - RLE compressed.
    //test_scanline_reader< bgra8_image_t, bmp_tag >( string( bmp_in + "g01bg.bmp" ).c_str() );

    // g04p4.bmp - 4-color grayscale palette
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g04p4.bmp" ).c_str() );

    // g08.bmp - basic 8bpp (256 color) image
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08.bmp" ).c_str() );

    // g08p256.bmp - biClrUsed=256, biClrImportant=0 [=256]
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08p256.bmp" ).c_str() );

    // g08pi256.bmp - biClrUsed=256, biClrImportant=256
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08pi256.bmp" ).c_str() );

    // g08pi64.bmp - biClrUsed=256, biClrImportant=64. It's barely possible that some
    // sophisticated viewers may display this image in grayscale, if there are a
    // limited number of colors available.
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08pi64.bmp" ).c_str() );

    // not supported
    // g08rle.bmp - RLE compressed.

    // g08os2.bmp - OS/2-style bitmap. This is an obsolete variety of BMP
    // that is still encountered sometimes. It has 3-byte palette
    // entries (instead of 4), and 16-bit width/height fields (instead of 32).
    test_scanline_reader< rgb8_image_t, bmp_tag >( string( bmp_in + "g08os2.bmp" ).c_str() );

    // g08res22.bmp - resolution 7874x7874 pixels/meter (200x200 dpi)
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08res22.bmp" ).c_str() );

    // g08res11.bmp - resolution 3937x3937 pixels/meter (100x100 dpi)
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08res11.bmp" ).c_str() );

    // g08res21.bmp resolution 7874x3937 pixels/meter (200x100 dpi).
    // Some programs (e.g. Imaging for Windows) may display this image
    // stretched vertically, which is the optimal thing to do if the
    // program is primarily a viewer, rather than an editor.
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08res21.bmp" ).c_str() );

    // g08s0.bmp - bits size not given (set to 0). This is legal for uncompressed bitmaps.
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08s0.bmp" ).c_str() );

    // g08offs.bmp - bfOffBits in header not set to the usual value.
    // There are 100 extra unused bytes between palette and bits.
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08offs.bmp" ).c_str() );

    // g08w126.bmp - size 126x63 (right and bottom slightly clipped)
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08w126.bmp" ).c_str() );

    // g08w125.bmp - size 125x62
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08w125.bmp" ).c_str() );

    // g08w124.bmp - size 124x61
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08w124.bmp" ).c_str() );

    // g08p64.bmp - 64-color grayscale palette
    test_scanline_reader< rgba8_image_t, bmp_tag >( string( bmp_in + "g08p64.bmp" ).c_str() );

    // g16def555.bmp - 15-bit color (1 bit wasted), biCompression=BI_RGB (no bitfields, defaults to 5-5-5)
    test_scanline_reader< rgb8_image_t, bmp_tag >( string( bmp_in + "g16def555.bmp" ).c_str() );

    // g16bf555.bmp - 15-bit color, biCompression=BI_BITFIELDS (bitfields indicate 5-5-5)
    test_scanline_reader< rgb8_image_t, bmp_tag >( string( bmp_in + "g16bf555.bmp" ).c_str() );

    // g16bf565.bmp - 16-bit color, biCompression=BI_BITFIELDS (bitfields indicate 5-6-5)
    test_scanline_reader< rgb8_image_t, bmp_tag >( string( bmp_in + "g16bf565.bmp" ).c_str() );

    // g24.bmp - 24-bit color (BGR)
    test_scanline_reader< bgr8_image_t, bmp_tag >( string( bmp_in + "g24.bmp" ).c_str() );

    // g32def.bmp - 24-bit color (8 bits wasted), biCompression=BI_RGB (no bitfields, defaults to BGRx)
    test_scanline_reader< bgra8_image_t, bmp_tag >( string( bmp_in + "g32def.bmp" ).c_str() );

    // g32bf.bmp - 24-bit color (8 bits wasted), biCompression=BI_BITFIELDS (bitfields indicate BGRx)
    test_scanline_reader< bgra8_image_t, bmp_tag >( string( bmp_in + "g32bf.bmp" ).c_str() );
}

BOOST_AUTO_TEST_CASE( partial_image_test )
{
    const std::string filename( bmp_in + "rgb.bmp" );

    {
        rgb8_image_t img;
        read_image( filename
                  , img
                  , image_read_settings< bmp_tag >( point_t( 0, 0 ), point_t( 50, 50 ) )
                  );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
        write_view( bmp_out + "rgb_partial.bmp"
                  , view( img )
                  , tag_t()
                  );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    }
}

#endif // BOOST_GIL_IO_USE_BMP_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
