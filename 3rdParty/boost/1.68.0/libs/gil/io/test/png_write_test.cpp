/*
    Copyright 2013 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

//#define BOOST_TEST_MODULE png_read_test_module
#include <boost/test/unit_test.hpp>

#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#define BOOST_GIL_IO_ENABLE_GRAY_ALPHA


#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem/convenience.hpp>

#include <boost/gil/extension/io/png.hpp>

#include <cstdint>
#include <iostream>

#include "color_space_write_test.hpp"
#include "paths.hpp"
#include "scanline_read_test.hpp"

using namespace std;
using namespace boost;
using namespace gil;
using namespace boost::gil::detail;
namespace fs = boost::filesystem;

typedef png_tag tag_t;

BOOST_AUTO_TEST_SUITE( gil_io_png_tests )

BOOST_AUTO_TEST_CASE( rgb_color_space_write_test )
{
    color_space_write_test< tag_t >( png_out + "rgb_color_space_test.png"
                                   , png_out + "bgr_color_space_test.png"
                                   );
}

BOOST_AUTO_TEST_SUITE_END()
