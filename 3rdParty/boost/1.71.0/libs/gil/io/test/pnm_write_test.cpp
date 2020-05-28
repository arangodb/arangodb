//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE pnm_write_test_module

#include <boost/gil/extension/io/pnm.hpp>

#include <boost/test/unit_test.hpp>

#include "color_space_write_test.hpp"
#include "mandel_view.hpp"
#include "paths.hpp"

using namespace std;
using namespace boost::gil;

using tag_t = pnm_tag;

BOOST_AUTO_TEST_SUITE( gil_io_pnm_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
BOOST_AUTO_TEST_CASE( write_test )
{
    mandel_view< rgb8_pixel_t >::type v = create_mandel_view( 200, 200
                                                            , rgb8_pixel_t( 0,   0, 255 )
                                                            , rgb8_pixel_t( 0, 255,   0 )
                                                            );

    // test writing all supported image types
    {
        using gray1_image_t = bit_aligned_image1_type<1, gray_layout_t>::type;
        gray1_image_t dst( 200, 200 );

        copy_and_convert_pixels( v, view( dst ));

        write_view( pnm_out + "p4_write_test.pnm"
                  , view( dst )
                  , pnm_tag()
                  );
    }

    {
        gray8_image_t dst( 200, 200 );

        copy_and_convert_pixels( v, view( dst ));

        write_view( pnm_out + "p5_write_test.pnm"
                  , view( dst )
                  , pnm_tag()
                  );
    }

    {
        write_view( pnm_out + "p6_write_test.pnm"
                  , v
                  , pnm_tag()
                  );
    }
}
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

BOOST_AUTO_TEST_CASE( rgb_color_space_write_test )
{
    color_space_write_test< pnm_tag >( pnm_out + "rgb_color_space_test.pnm"
                                     , pnm_out + "bgr_color_space_test.pnm"
                                     );
}

BOOST_AUTO_TEST_SUITE_END()
