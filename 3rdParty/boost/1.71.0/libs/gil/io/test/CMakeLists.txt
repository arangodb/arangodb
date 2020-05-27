#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: Configuring IO extension tests")

set(_formats
  bmp
  jpeg
  png
  pnm
  simple
  targa
  tiff)

if(GIL_ENABLE_EXT_IO_RAW)
  list(APPEND _formats raw)
endif()

foreach(_name ${_formats})
  set(_target test_ext_io_${_name})

  add_executable(${_target} "")
  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  target_compile_definitions(${_target}
    PRIVATE
      BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
      BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES)
  add_test(test.ext.io.${_name} ${_target})

  unset(_name)
  unset(_target)
endforeach()

unset(_formats)

target_sources(test_ext_io_simple
  PRIVATE
    all_formats_test.cpp)

target_sources(test_ext_io_bmp
  PRIVATE
    bmp_old_test.cpp bmp_read_test.cpp bmp_test.cpp bmp_write_test.cpp)

target_sources(test_ext_io_jpeg
  PRIVATE
    jpeg_test.cpp jpeg_old_test.cpp jpeg_read_test.cpp jpeg_write_test.cpp)

target_sources(test_ext_io_png
  PRIVATE
    png_test.cpp png_old_test.cpp png_file_format_test.cpp png_read_test.cpp)

target_sources(test_ext_io_pnm
  PRIVATE
    pnm_test.cpp pnm_old_test.cpp pnm_read_test.cpp pnm_write_test.cpp)

if(GIL_ENABLE_EXT_IO_RAW)
  target_sources(test_ext_io_raw
    PRIVATE
      raw_test.cpp)
endif()

target_sources(test_ext_io_targa
  PRIVATE
    targa_test.cpp targa_old_test.cpp targa_read_test.cpp targa_write_test.cpp)

target_sources(test_ext_io_tiff
  PRIVATE
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
