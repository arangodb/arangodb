//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/pnm.hpp>

#include <boost/core/lightweight_test.hpp>

#include "color_space_write_test.hpp"
#include "mandel_view.hpp"
#include "paths.hpp"

namespace gil = boost::gil;;

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
void test_write()
{
    mandel_view<gil::rgb8_pixel_t>::type v =
        create_mandel_view(200, 200, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0));

    // test writing all supported image types
    {
        using gray1_image_t = gil::bit_aligned_image1_type<1, gil::gray_layout_t>::type;
        gray1_image_t dst(200, 200);
        gil::copy_and_convert_pixels(v, gil::view(dst));

        gil::write_view(pnm_out + "p4_write_test.pnm", gil::view(dst), gil::pnm_tag());
    }
    {
        gil::gray8_image_t dst(200, 200);
        gil::copy_and_convert_pixels(v, gil::view(dst));

        gil::write_view(pnm_out + "p5_write_test.pnm", gil::view(dst), gil::pnm_tag());
    }
    {
        gil::write_view(pnm_out + "p6_write_test.pnm", v, gil::pnm_tag());
    }
}
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

void test_rgb_color_space_write()
{
    color_space_write_test<gil::pnm_tag>(
        pnm_out + "rgb_color_space_test.pnm", pnm_out + "bgr_color_space_test.pnm");
}

int main()
{
    test_rgb_color_space_write();

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    test_write();
#endif

    return boost::report_errors();
}
