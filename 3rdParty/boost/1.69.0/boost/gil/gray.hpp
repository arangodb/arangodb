//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_GRAY_HPP
#define BOOST_GIL_GRAY_HPP

#include <boost/mpl/range_c.hpp>
#include <boost/mpl/vector_c.hpp>
#include <boost/type_traits.hpp>

#include <boost/gil/utilities.hpp>

namespace boost { namespace gil {

/// \ingroup ColorNameModel
/// \brief Gray
struct gray_color_t {};

/// \ingroup ColorSpaceModel
typedef mpl::vector1<gray_color_t> gray_t;

/// \ingroup LayoutModel
typedef layout<gray_t> gray_layout_t;

} }  // namespace boost::gil

#endif

