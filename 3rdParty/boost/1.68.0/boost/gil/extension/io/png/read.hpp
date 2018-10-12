/*
    Copyright 2007-2008 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_PNG_READ_HPP
#define BOOST_GIL_EXTENSION_IO_PNG_READ_HPP

#define BOOST_GIL_EXTENSION_IO_PNG_READ_ENABLED

////////////////////////////////////////////////////////////////////////////////////////
/// \file               
/// \brief
/// \author Christian Henning \n
///         
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/gil/extension/io/png/tags.hpp>
#include <boost/gil/extension/io/png/detail/supported_types.hpp>
#include <boost/gil/extension/io/png/detail/read.hpp>
#include <boost/gil/extension/io/png/detail/scanline_read.hpp>

#include <boost/gil/io/get_reader.hpp>
#include <boost/gil/io/make_backend.hpp>
#include <boost/gil/io/make_reader.hpp>
#include <boost/gil/io/make_dynamic_image_reader.hpp>
#include <boost/gil/io/make_scanline_reader.hpp>

#include <boost/gil/io/read_image.hpp>
#include <boost/gil/io/read_view.hpp>
#include <boost/gil/io/read_image_info.hpp>
#include <boost/gil/io/read_and_convert_image.hpp>
#include <boost/gil/io/read_and_convert_view.hpp>

#include <boost/gil/io/scanline_read_iterator.hpp>

#endif
