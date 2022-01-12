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

#include <string>

#include "paths.hpp"
#include "scanline_read_test.hpp"

namespace gil = boost::gil;

template <typename Image>
void write(Image& img, std::string const& file_name)
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(pnm_out + file_name, gil::view(img), gil::pnm_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

template <typename Image>
void test_pnm_scanline_reader(std::string filename)
{
    test_scanline_reader<Image, gil::pnm_tag>(std::string(pnm_in + filename).c_str());
}

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
void test_read_header()
{
    using backend_t   = gil::get_reader_backend<std::string const, gil::pnm_tag>::type;
    backend_t backend = gil::read_image_info(pnm_filename, gil::pnm_tag());

    BOOST_TEST_EQ(backend._info._type, gil::pnm_image_type::color_bin_t::value);
    BOOST_TEST_EQ(backend._info._width, 256u);
    BOOST_TEST_EQ(backend._info._height, 256u);
    BOOST_TEST_EQ(backend._info._max_value, 255u);
}

#ifdef BOOST_GIL_IO_USE_PNM_TEST_SUITE_IMAGES
void test_read_reference_images()
{
    // p1.pnm
    {
        gil::gray8_image_t img;
        gil::read_image(pnm_in + "p1.pnm", img, gil::pnm_tag());
        BOOST_TEST_EQ(view(img).width(), 200u);
        BOOST_TEST_EQ(view(img).height(), 200u);

        write(img, "p1.pnm");
        test_pnm_scanline_reader<gil::gray8_image_t>("p1.pnm");
    }
    // p2.pnm
    {
        gil::gray8_image_t img;
        gil::read_image(pnm_in + "p2.pnm", img, gil::pnm_tag());
        BOOST_TEST_EQ(view(img).width(), 200u);
        BOOST_TEST_EQ(view(img).height(), 200u);

        write(img, "p2.pnm");
        test_pnm_scanline_reader<gil::gray8_image_t>("p2.pnm");
    }
    // p3.pnm
    {
        gil::rgb8_image_t img;
        gil::read_image(pnm_in + "p3.pnm", img, gil::pnm_tag());
        BOOST_TEST_EQ(view(img).width(), 256u);
        BOOST_TEST_EQ(view(img).height(), 256u);

        write(img, "p3.pnm");
        test_pnm_scanline_reader<rgb8_image_t>("p3.pnm");
    }
    // p4.pnm
    {
        gil::gray1_image_t img;
        gil::read_image(pnm_in + "p4.pnm", img, gil::pnm_tag());
        BOOST_TEST_EQ(view(img).width(), 200u);
        BOOST_TEST_EQ(view(img).height(), 200u);

        write(img, "p4.pnm");
        test_pnm_scanline_reader<gil::gray1_image_t>("p4.pnm");
    }
    // p5.pnm
    {
        gil::gray8_image_t img;
        gil::read_image(pnm_in + "p5.pnm", img, gil::pnm_tag());
        BOOST_TEST_EQ(view(img).width(), 200u);
        BOOST_TEST_EQ(view(img).height(), 200u);

        write(img, "p5.pnm");
        test_pnm_scanline_reader<gil::gray8_image_t>("p5.pnm");
    }
    // p6.pnm
    {
        gil::rgb8_image_t img;
        gil::read_image(pnm_in + "p6.pnm", img, gil::pnm_tag());
        BOOST_TEST_EQ(view(img).width(), 256u);
        BOOST_TEST_EQ(view(img).height(), 256u);

        write(img, "p6.pnm");
        test_pnm_scanline_reader<gil::rgb8_image_t>("p6.pnm");
    }
}
#endif // BOOST_GIL_IO_USE_PNM_TEST_SUITE_IMAGES

int main()
{
    test_read_header();

#ifdef BOOST_GIL_IO_USE_PNM_TEST_SUITE_IMAGES
    test_read_reference_images();
#endif

    return boost::report_errors();
}
#else
int main() {}
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
