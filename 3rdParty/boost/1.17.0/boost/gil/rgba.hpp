//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_RGBA_HPP
#define BOOST_GIL_RGBA_HPP

#include <boost/gil/planar_pixel_iterator.hpp>
#include <boost/gil/rgb.hpp>

#include <boost/mpl/contains.hpp>
#include <boost/mpl/vector.hpp>

#include <cstddef>

namespace boost { namespace gil {

/// \ingroup ColorNameModel
/// \brief Alpha
struct alpha_t {};

/// \ingroup ColorSpaceModel
using rgba_t = mpl::vector4<red_t,green_t,blue_t,alpha_t>;

/// \ingroup LayoutModel
using rgba_layout_t = layout<rgba_t>;
/// \ingroup LayoutModel
using bgra_layout_t = layout<rgba_t, mpl::vector4_c<int,2,1,0,3>>;
/// \ingroup LayoutModel
using argb_layout_t = layout<rgba_t, mpl::vector4_c<int,1,2,3,0>>;
/// \ingroup LayoutModel
using abgr_layout_t = layout<rgba_t, mpl::vector4_c<int,3,2,1,0>>;

/// \ingroup ImageViewConstructors
/// \brief from raw RGBA planar data
template <typename IC>
inline
typename type_from_x_iterator<planar_pixel_iterator<IC,rgba_t> >::view_t
planar_rgba_view(std::size_t width, std::size_t height,
                 IC r, IC g, IC b, IC a,
                 std::ptrdiff_t rowsize_in_bytes)
{
    using view_t = typename type_from_x_iterator<planar_pixel_iterator<IC,rgba_t> >::view_t;
    return RView(width, height,
                 typename view_t::locator(planar_pixel_iterator<IC,rgba_t>(r,g,b,a),
                                         rowsize_in_bytes));
}

}} // namespace boost::gil

#endif
