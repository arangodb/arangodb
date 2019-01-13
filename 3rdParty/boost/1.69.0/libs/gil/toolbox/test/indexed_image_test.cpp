//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/image_types/indexed_image.hpp>

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost;
using namespace gil;


BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( index_image_test )
{
    {
        indexed_image< uint8_t, rgb8_pixel_t > img( 640, 480 );
        fill_pixels( view( img ), rgb8_pixel_t( 255, 0, 0 ));

        rgb8_pixel_t p = *view( img ).xy_at( 10, 10 );
    }

    {
        typedef indexed_image< gray8_pixel_t, rgb8_pixel_t > image_t;

        image_t img( 640, 480, 256 );


#if __cplusplus >= 201103L
        generate_pixels( img.get_indices_view()
                       , [] () -> uint8_t
                        {
                            static uint8_t i = 0;
                            i = (i == 256) ? 0 : (i + 1);

                            return gray8_pixel_t( i );
                        }
                       );


        generate_pixels( img.get_palette_view()
                       , [] () ->rgb8_pixel_t
                        {
                            static uint8_t i = 0;
                            i = (i == 256) ? 0 : (i + 1);

                            return rgb8_pixel_t( i, i, i );
                        }
                       );
#else

        image_t::indices_view_t indices = img.get_indices_view();

        for( image_t::indices_view_t::iterator it = indices.begin(); it != indices.end(); ++it )
        {
            static uint8_t i = 0;
            i = (i == 256) ? 0 : (i + 1);

            *it = gray8_pixel_t( i );
        }

        image_t::palette_view_t colors = img.get_palette_view();
        for( image_t::palette_view_t::iterator it = colors.begin(); it != colors.end(); ++it )
        {
            static uint8_t i = 0;
            i = (i == 256) ? 0 : (i + 1);

            *it = rgb8_pixel_t( i, i, i );
        }
#endif

        gray8_pixel_t index = *img.get_indices_view().xy_at( 10   , 1 );
        rgb8_pixel_t  color = *img.get_palette_view().xy_at( index, 0 );

        rgb8_pixel_t p = *view( img ).xy_at( 10, 1 );
    }

    {
        typedef indexed_image< gray8_pixel_t, rgb8_pixel_t > image_t;
        image_t img( 640, 480, 256 );

#if __cplusplus >= 201103L
        generate_pixels( img.get_indices_view()
                       , [] () -> uint8_t
                       {
                            static uint8_t i = 0;
                            i = (i == 256) ? 0 : (i + 1);
                            return i;
                       }
                       );


        generate_pixels( img.get_palette_view()
                       , [] () ->rgb8_pixel_t
                       {
                          static uint8_t i = 0;
                           i = (i == 256) ? 0 : (i + 1);

                          return rgb8_pixel_t( i, i, i );
                       }
                       );
#else
        image_t::indices_view_t indices = img.get_indices_view();
        for( image_t::indices_view_t::iterator it = indices.begin(); it != indices.end(); ++it )
        {
            static uint8_t i = 0;
            i = (i == 256) ? 0 : (i + 1);

            *it = gray8_pixel_t( i );
        }

        image_t::palette_view_t colors = img.get_palette_view();
        for( image_t::palette_view_t::iterator it = colors.begin(); it != colors.end(); ++it )
        {
            static uint8_t i = 0;
            i = (i == 256) ? 0 : (i + 1);

            *it = rgb8_pixel_t( i, i, i );
        }
#endif

        uint8_t      index = *img.get_indices_view().xy_at( 10   , 1 );
        rgb8_pixel_t color = *img.get_palette_view().xy_at( index, 0 );

        rgb8_pixel_t p = *view( img ).xy_at( 10, 1 );
    }

    {
        typedef indexed_image< uint8_t, rgb8_pixel_t > image_t;
        image_t img( 640, 480, 256 );

        for( image_t::y_coord_t y = 0; y < view( img ).height(); ++y )
        {
            image_t::view_t::x_iterator it = view( img ).row_begin( y );

            for( image_t::x_coord_t x = 0; x < view( img ).width(); ++x )
            {
                rgb8_pixel_t p = *it;
                it++;
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(index_image_view_test)
{
    // generate some data
    std::size_t const width = 640;
    std::size_t const height = 480;
    std::size_t const num_colors = 3;
    std::uint8_t const index = 2;

    // indices
    vector<uint8_t> indices(width * height, index);

    // colors
    vector<rgb8_pixel_t> palette(num_colors);
    palette[0] = rgb8_pixel_t(10, 20, 30);
    palette[1] = rgb8_pixel_t(40, 50, 60);
    palette[2] = rgb8_pixel_t(70, 80, 90);

    // create image views from raw memory
    typedef gray8_image_t::view_t::locator indices_loc_t;
    typedef rgb8_image_t::view_t::locator palette_loc_t;


    auto indices_view = interleaved_view(width, height
        , (gray8_image_t::view_t::x_iterator) indices.data()
        , width // row size in bytes
    );

    auto palette_view = interleaved_view(100, 1
        , (rgb8_image_t::view_t::x_iterator) palette.data()
        , num_colors * 3 // row size in bytes
    );

    auto ii_view = view(indices_view, palette_view);

    auto p = ii_view(point_t(0, 0));
    auto q = *ii_view.at(point_t(0, 0));

    assert(get_color(p, red_t()) == 70);
    assert(get_color(p, green_t()) == 80);
    assert(get_color(p, blue_t()) == 90);

    assert(get_color(q, red_t()) == 70);
    assert(get_color(q, green_t()) == 80);
    assert(get_color(q, blue_t()) == 90);
}

BOOST_AUTO_TEST_SUITE_END()
