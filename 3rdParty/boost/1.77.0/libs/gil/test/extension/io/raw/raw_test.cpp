//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#include <boost/gil.hpp>
#include <boost/gil/extension/io/raw.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include <fstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

namespace fs = boost::filesystem;
namespace gil = boost::gil;
namespace mp11 = boost::mp11;

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

void test_read_image_info_using_string()
{
    {
        /// raw_tag reader's can only constructed with char*, std::string, and LibRaw object

        using backend_t = gil::get_reader_backend<char const *, gil::raw_tag>::type;
        backend_t b = gil::make_reader_backend(raw_filename.c_str(),
            gil::image_read_settings<gil::raw_tag>());
        backend_t backend = gil::read_image_info(raw_filename, gil::raw_tag());

        BOOST_TEST_EQ(backend._info._width, 2176);
        BOOST_TEST_EQ(backend._info._height, 1448);
    }
    {
        fs::path my_path(raw_filename);
        using backend_t = gil::get_reader_backend<fs::path, gil::raw_tag>::type;
        backend_t backend = gil::read_image_info(my_path, gil::raw_tag());

        BOOST_TEST_EQ(backend._info._width, 2176);
        BOOST_TEST_EQ(backend._info._height, 1448);
    }
}

void test_read_image()
{
    {
        gil::rgb8_image_t img;
        gil::read_image(raw_filename, img, gil::raw_tag());

        BOOST_TEST_EQ(img.width(), 2176);
        BOOST_TEST_EQ(img.height(), 1448);
    }

    {
        fs::path my_path(raw_filename);
        gil::rgb8_image_t img;
        gil::read_image(my_path, img, gil::raw_tag());

        BOOST_TEST_EQ(img.width(), 2176);
        BOOST_TEST_EQ(img.height(), 1448);
    }
}

void test_read_and_convert_image()
{
    gil::rgb8_image_t img;
    gil::read_and_convert_image(raw_filename, img, gil::raw_tag());

    BOOST_TEST_EQ(img.width(), 2176);
    BOOST_TEST_EQ(img.height(), 1448);
}

void test_read_view()
{
    gil::rgb8_image_t img(2176, 1448);
    gil::read_view(raw_filename, gil::view(img), gil::raw_tag());
}

void test_read_and_convert_view()
{
    gil::rgb8_image_t img(2176, 1448);
    gil::read_and_convert_view(raw_filename, gil::view(img), gil::raw_tag());
}

void test_subimage()
{
    run_subimage_test<gil::rgb8_image_t, gil::raw_tag>(raw_filename, gil::point_t(0, 0), gil::point_t(127, 1));
    run_subimage_test<gil::rgb8_image_t, gil::raw_tag>(raw_filename, gil::point_t(39, 7), gil::point_t(50, 50));
}

void test_dynamic_image()
{
  using my_img_types = mp11::mp_list
      <
          gil::gray8_image_t,
          gil::gray16_image_t,
          gil::rgb8_image_t,
          gil::rgba8_image_t
      >;

  gil::any_image<my_img_types> image;
  gil::read_image(raw_filename.c_str(), image, gil::raw_tag());
}

int main()
{
    test_read_image_info_using_string();
    test_read_image();
    test_read_and_convert_image();
    test_read_view();
    test_read_and_convert_view();
    //test_subimage();
    test_dynamic_image();

    return boost::report_errors();
}

#else
int main() {}
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
