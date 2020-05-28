//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_TEST_PATHS_HPP
#define BOOST_GIL_IO_TEST_PATHS_HPP

// Disable warning: conversion to 'std::atomic<int>::__integral_type {aka int}' from 'long int' may alter its value
#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>

#if defined(BOOST_CLANG)
#pragma clang diagnostic pop
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic pop
#endif

// `base` holds the path to ../.., i.e. the directory containing `test_images`
static const std::string base = (boost::filesystem::absolute(boost::filesystem::path(__FILE__)).parent_path().parent_path().string()) + "/";

static const std::string bmp_in  = base + "test_images/bmp/";
static const std::string bmp_out = base + "output/bmp/";

static const std::string jpeg_in  = base + "test_images/jpg/";
static const std::string jpeg_out = base + "output/jpeg/";

static const std::string png_base_in = base + "test_images/png/";
static const std::string png_in      = png_base_in + "PngSuite/";
static const std::string png_out     = base + "output/png/";

static const std::string pnm_in  = base + "test_images/pnm/";
static const std::string pnm_out = base + "output/pnm/";

static const std::string raw_in  = base + "test_images/raw/";

static const std::string targa_in  = base + "test_images/targa/";
static const std::string targa_out = base + "output/targa/";

static const std::string tiff_in    = base + "test_images/tiff/";
static const std::string tiff_out   = base + "output/tiff/";
static const std::string tiff_in_GM = tiff_in + "graphicmagick/";

static const std::string bmp_filename  ( bmp_in      + "test.bmp"               );
static const std::string jpeg_filename ( jpeg_in     + "test.jpg"               );
static const std::string png_filename  ( png_base_in + "test.png"               );
static const std::string pnm_filename  ( pnm_in      + "rgb.pnm"                );
static const std::string raw_filename  ( raw_in      + "RAW_CANON_D30_SRGB.CRW" );
static const std::string targa_filename( targa_in    + "24BPP_compressed.tga"   );
static const std::string tiff_filename ( tiff_in     + "test.tif"               );

#endif // BOOST_GIL_IO_TEST_PATHS_HPP
