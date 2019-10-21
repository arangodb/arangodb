//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_TEST_MODULE raw_test
#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

#include <boost/gil.hpp>
#include <boost/gil/extension/io/raw.hpp>

#include <boost/test/unit_test.hpp>

#include <fstream>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

using namespace std;
using namespace boost;
using namespace gil;
namespace fs = boost::filesystem;

using tag_t = raw_tag;

BOOST_AUTO_TEST_SUITE( gil_io_raw_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( read_image_info_using_string )
{
  {
    /// raw_tag reader's can only constructed with char*, std::string, and LibRaw object

    using backend_t = get_reader_backend<char const*, tag_t>::type;

    backend_t b = make_reader_backend(raw_filename.c_str(),
				      image_read_settings<raw_tag>());

    backend_t backend = read_image_info(raw_filename, tag_t());

    BOOST_CHECK_EQUAL( backend._info._width , 2176 );
    BOOST_CHECK_EQUAL( backend._info._height, 1448 );
  }

    {
        fs::path my_path( raw_filename );

        using backend_t = get_reader_backend<fs::path, tag_t>::type;

        backend_t backend = read_image_info(my_path, tag_t());

        BOOST_CHECK_EQUAL( backend._info._width , 2176 );
        BOOST_CHECK_EQUAL( backend._info._height, 1448 );
    }
}

BOOST_AUTO_TEST_CASE( read_image_test )
{
  {
    rgb8_image_t img;
    read_image( raw_filename, img, tag_t() );

    BOOST_CHECK_EQUAL( img.width() , 2176 );
    BOOST_CHECK_EQUAL( img.height(), 1448 );
  }

  {
    fs::path my_path( raw_filename );

    rgb8_image_t img;
    read_image( my_path, img, tag_t() );

    BOOST_CHECK_EQUAL( img.width() , 2176 );
    BOOST_CHECK_EQUAL( img.height(), 1448 );
  }
}

BOOST_AUTO_TEST_CASE( read_and_convert_image_test )
{
  rgb8_image_t img;
  read_and_convert_image( raw_filename, img, tag_t() );

  BOOST_CHECK_EQUAL( img.width() , 2176 );
  BOOST_CHECK_EQUAL( img.height(), 1448 );
}

BOOST_AUTO_TEST_CASE( read_view_test )
{
  rgb8_image_t img( 2176, 1448 );
  read_view( raw_filename, view( img ), tag_t() );
}

BOOST_AUTO_TEST_CASE( read_and_convert_view_test )
{
  rgb8_image_t img( 2176, 1448 );
  read_and_convert_view( raw_filename, view( img ), tag_t() );
}

// BOOST_AUTO_TEST_CASE( subimage_test )
// {
//   run_subimage_test<rgb8_image_t, tag_t>(raw_filename, point_t(0, 0), point_t(127, 1));
//   run_subimage_test<rgb8_image_t, tag_t>(raw_filename, point_t(39, 7), point_t(50, 50));
// }

BOOST_AUTO_TEST_CASE( dynamic_image_test )
{
  using my_img_types = mpl::vector
      <
          gray8_image_t,
          gray16_image_t,
          rgb8_image_t,
          rgba8_image_t
      >;

  any_image< my_img_types > runtime_image;

  read_image(raw_filename.c_str(), runtime_image, tag_t());
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
