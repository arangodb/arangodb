#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: configuring IO extension tests")

# Unfortunately, ALIAS of imported target is not supported
# see https://github.com/conan-io/conan/issues/2125
list(APPEND _boost_deps Boost::disable_autolinking)
list(APPEND _boost_deps Boost::filesystem)
list(APPEND _boost_deps Boost::unit_test_framework)
if (GIL_USE_CONAN)
  list(APPEND _jpeg_deps CONAN_PKG::libjpeg)
  list(APPEND _png_deps CONAN_PKG::libpng)
  list(APPEND _tiff_deps CONAN_PKG::libtiff)
else()
  list(APPEND _jpeg_deps ${JPEG_LIBRARIES})
  list(APPEND _png_deps PNG::PNG)
  list(APPEND _tiff_deps TIFF::TIFF)
endif()

file(GLOB_RECURSE _headers
  ${PROJECT_SOURCE_DIR}/include/boost/gil/extension/io/*.hpp)
list(APPEND _headers ${_boost_gil_headers})

add_definitions(-DBOOST_GIL_IO_TEST_ALLOW_READING_IMAGES=1)
add_definitions(-DBOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES=1)
if(NOT MSVC)
  add_definitions(-DBOOST_TEST_DYN_LINK)
endif()

add_executable(gil_test_ext_io_simple "")
target_sources(gil_test_ext_io_simple PRIVATE ${_headers} all_formats_test.cpp)
target_link_libraries(gil_test_ext_io_simple PRIVATE
  ${_boost_deps} ${_jpeg_deps} ${_png_deps} ${_tiff_deps})
if (JPEG_LIBRARIES AND JPEG_INCLUDE_DIR)
  target_include_directories(gil_test_ext_io_simple PRIVATE ${JPEG_INCLUDE_DIR})
endif()
add_test(tests.ext.io.simple gil_test_ext_io_simple)

add_executable(gil_test_ext_io_bmp "")
target_sources(gil_test_ext_io_bmp PRIVATE ${_headers}
  bmp_old_test.cpp bmp_read_test.cpp bmp_test.cpp bmp_write_test.cpp)
target_link_libraries(gil_test_ext_io_bmp PRIVATE ${_boost_deps})
add_test(tests.ext.io.bmp gil_test_ext_io_bmp)

add_executable(gil_test_ext_io_jpeg "")
target_sources(gil_test_ext_io_jpeg PRIVATE ${_headers}
  jpeg_test.cpp jpeg_old_test.cpp jpeg_read_test.cpp jpeg_write_test.cpp)
target_link_libraries(gil_test_ext_io_jpeg PRIVATE ${_boost_deps} ${_jpeg_deps})
if (JPEG_LIBRARIES AND JPEG_INCLUDE_DIR)
  target_include_directories(gil_test_ext_io_jpeg PRIVATE ${JPEG_INCLUDE_DIR})
endif()
add_test(tests.ext.io.jpeg gil_test_ext_io_jpeg)

add_executable(gil_test_ext_io_png "")
target_sources(gil_test_ext_io_png PRIVATE ${_headers}
  png_test.cpp png_old_test.cpp png_file_format_test.cpp png_read_test.cpp)
target_link_libraries(gil_test_ext_io_png PRIVATE ${_boost_deps} ${_png_deps})
add_test(tests.ext.io.png gil_test_ext_io_png)

add_executable(gil_test_ext_io_pnm "")
target_sources(gil_test_ext_io_pnm PRIVATE ${_headers}
  pnm_test.cpp pnm_old_test.cpp pnm_read_test.cpp pnm_write_test.cpp)
target_link_libraries(gil_test_ext_io_pnm PRIVATE ${_boost_deps} ${_png_deps})
add_test(tests.ext.io.pnm gil_test_ext_io_pnm)

add_executable(gil_test_ext_io_targa "")
target_sources(gil_test_ext_io_targa PRIVATE ${_headers}
  targa_test.cpp targa_old_test.cpp targa_read_test.cpp targa_write_test.cpp)
target_link_libraries(gil_test_ext_io_targa PRIVATE ${_boost_deps})
add_test(tests.ext.io.targa gil_test_ext_io_targa)

add_executable(gil_test_ext_io_tiff "")
target_sources(gil_test_ext_io_tiff PRIVATE
  ${_headers}
  tiff_file_format_test.cpp
  tiff_old_test.cpp
  tiff_subimage_test.cpp
  tiff_test.cpp
  tiff_tiled_float_test.cpp
  tiff_tiled_minisblack_test_1-10.cpp
  tiff_tiled_minisblack_test_11-20.cpp
  tiff_tiled_minisblack_test_21-31_32-64.cpp
  tiff_tiled_minisblack_write_test_1-10.cpp
  tiff_tiled_minisblack_write_test_11-20.cpp
  tiff_tiled_minisblack_write_test_21-31_32-64.cpp
  tiff_tiled_palette_test_1-8.cpp
  tiff_tiled_palette_test_8-16.cpp
  tiff_tiled_palette_write_test_1-8.cpp
  tiff_tiled_palette_write_test_8-16.cpp
  tiff_tiled_rgb_contig_test_1-10.cpp
  tiff_tiled_rgb_contig_test_11-20.cpp
  tiff_tiled_rgb_contig_test_21-31_32_64.cpp
  tiff_tiled_rgb_contig_write_test_1-10.cpp
  tiff_tiled_rgb_contig_write_test_11-20.cpp
  tiff_tiled_rgb_contig_write_test_21-31_32_64.cpp
  tiff_tiled_rgb_planar_test_1-10.cpp
  tiff_tiled_rgb_planar_test_11-20.cpp
  tiff_tiled_rgb_planar_test_21-31_32_64.cpp
  tiff_tiled_test.cpp
  tiff_write_test.cpp)
target_link_libraries(gil_test_ext_io_tiff PRIVATE ${_boost_deps} ${_tiff_deps} ${_tiffxx_library})
target_include_directories(gil_test_ext_io_tiff PRIVATE ${_tiffxx_include_dir})
add_test(tests.ext.io.tiff gil_test_ext_io_tiff)
