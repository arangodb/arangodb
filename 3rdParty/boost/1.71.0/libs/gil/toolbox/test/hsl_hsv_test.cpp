//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/hsl.hpp>
#include <boost/gil/extension/toolbox/color_spaces/hsv.hpp>

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( hsl_hsv_test )
{
    {
        rgb8_pixel_t p( 128, 0, 128 );

        hsl32f_pixel_t h;

        color_convert( p, h );
        color_convert( h, p );
    }

    {
        size_t width  = 640;
        size_t height = 480;

        hsl32f_image_t hsl_img( width, height );
        hsv32f_image_t hsv_img( width, height );

        for( size_t y = 0; y < height; y++ )
        {
            hsl32f_view_t::x_iterator hsl_x_it = view( hsl_img ).row_begin( y );
            hsv32f_view_t::x_iterator hsv_x_it = view( hsv_img ).row_begin( y );

            float v = static_cast<float>( height -  y )
                    / height;

            for( size_t x = 0; x < width; x++ )
            {
                float hue = ( x + 1.f ) / width;

                hsl_x_it[x] = hsl32f_pixel_t( hue, 1.0, v );
                hsv_x_it[x] = hsv32f_pixel_t( hue, 1.0, v );
            }
        }
    }

    {
        rgb8_image_t rgb_img( 640, 480 );
        fill_pixels( view(rgb_img), rgb8_pixel_t( 255, 128, 64 ));
        hsl32f_image_t hsl_img( view( rgb_img ).dimensions() );

        copy_pixels( color_converted_view<hsl32f_pixel_t>( view( rgb_img ))
                    , view( hsl_img ));
    }

    {
        rgb8_image_t rgb_img( 640, 480 );
        fill_pixels( view(rgb_img), rgb8_pixel_t( 255, 128, 64 ));
        hsv32f_image_t hsv_img( view( rgb_img ).dimensions() );

        copy_pixels( color_converted_view<hsv32f_pixel_t>( view( rgb_img ))
                    , view( hsv_img ));
    }
}

BOOST_AUTO_TEST_SUITE_END()
