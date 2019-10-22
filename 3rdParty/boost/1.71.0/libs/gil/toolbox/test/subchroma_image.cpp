// Copyright 2013 Christian Henning
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/// \brief Unit test for subchroma_image type.

#include <boost/test/unit_test.hpp>

#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/ycbcr.hpp>
#include <boost/gil/extension/toolbox/image_types/subchroma_image.hpp>

using namespace std;
using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( subchroma_image_test )
{
    {
        ycbcr_601_8_pixel_t a( 10, 20, 30 );
        rgb8_pixel_t b;
        bgr8_pixel_t c;

        color_convert( a, b );
        color_convert( a, c );
        BOOST_ASSERT( static_equal( b, c ));

        color_convert( b, a );
    }

    {
        ycbcr_709_8_pixel_t a( 10, 20, 30 );
        rgb8_pixel_t b;
        bgr8_pixel_t c;

        color_convert( a, b );
        color_convert( a, c );
        BOOST_ASSERT( static_equal( b, c ));

        color_convert( b, a );
    }

    {
        using pixel_t = rgb8_pixel_t;
        using image_t = subchroma_image<pixel_t>;

        image_t img( 640, 480 );

        fill_pixels( view( img )
                   , pixel_t( 10, 20, 30 )
                   );
    }

    {
        using pixel_t = rgb8_pixel_t;

        subchroma_image<pixel_t, mpl::vector_c<int, 4, 4, 4>> a(640, 480);
        static_assert(a.ss_X == 1 && a.ss_Y == 1, "");
        subchroma_image<pixel_t, mpl::vector_c<int, 4, 4, 0>> b(640, 480);
        static_assert(b.ss_X == 1 && b.ss_Y == 2, "");
        subchroma_image<pixel_t, mpl::vector_c<int, 4, 2, 2>> c(640, 480);
        static_assert(c.ss_X == 2 && c.ss_Y == 1, "");
        subchroma_image<pixel_t, mpl::vector_c<int, 4, 2, 0>> d(640, 480);
        static_assert(d.ss_X == 2 && d.ss_Y == 2, "");
        subchroma_image<pixel_t, mpl::vector_c<int, 4, 1, 1>> e(640, 480);
        static_assert(e.ss_X == 4 && e.ss_Y == 1, "");
        subchroma_image<pixel_t, mpl::vector_c<int, 4, 1, 0>> f(640, 480);
        static_assert(f.ss_X == 4 && f.ss_Y == 2, "");

        fill_pixels( view( a ), pixel_t( 10, 20, 30 ) );
        fill_pixels( view( b ), pixel_t( 10, 20, 30 ) );
        fill_pixels( view( c ), pixel_t( 10, 20, 30 ) );
        fill_pixels( view( d ), pixel_t( 10, 20, 30 ) );
        fill_pixels( view( e ), pixel_t( 10, 20, 30 ) );
        fill_pixels( view( f ), pixel_t( 10, 20, 30 ) );

    }

    {
        using pixel_t = ycbcr_601_8_pixel_t;
        using factors_t = mpl::vector_c<int, 4, 2, 2>;
        using image_t = subchroma_image<pixel_t, factors_t>;

        std::size_t y_width     = 640;
        std::size_t y_height    = 480;

        std::size_t image_size = ( y_width * y_height )
                               + ( y_width / image_t::ss_X ) * ( y_height / image_t::ss_Y )
                               + ( y_width / image_t::ss_X ) * ( y_height / image_t::ss_Y );

        vector< unsigned char > data( image_size );

        image_t::view_t v = subchroma_view< pixel_t, factors_t >( y_width
                                                                , y_height
                                                                , &data.front()
                                                                );
        rgb8_pixel_t p;
        p = *v.xy_at( 0, 0 );
    }
}

BOOST_AUTO_TEST_SUITE_END()
