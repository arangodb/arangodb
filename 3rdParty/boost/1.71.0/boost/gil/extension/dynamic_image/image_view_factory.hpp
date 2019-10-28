//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_DYNAMIC_IMAGE_IMAGE_VIEW_FACTORY_HPP
#define BOOST_GIL_EXTENSION_DYNAMIC_IMAGE_IMAGE_VIEW_FACTORY_HPP

#include <boost/gil/extension/dynamic_image/any_image_view.hpp>

#include <boost/gil/dynamic_step.hpp>
#include <boost/gil/image_view_factory.hpp>
#include <boost/gil/point.hpp>

namespace boost { namespace gil {

// Methods for constructing any image views from other any image views
// Extends image view factory to runtime type-specified views (any_image_view)

namespace detail {
template <typename Result> struct flipped_up_down_view_fn {
    using result_type = Result;
    template <typename View> result_type operator()(const View& src) const { return result_type(flipped_up_down_view(src)); }
};
template <typename Result> struct flipped_left_right_view_fn {
    using result_type = Result;
    template <typename View> result_type operator()(const View& src) const { return result_type(flipped_left_right_view(src)); }
};
template <typename Result> struct rotated90cw_view_fn {
    using result_type = Result;
    template <typename View> result_type operator()(const View& src) const { return result_type(rotated90cw_view(src)); }
};
template <typename Result> struct rotated90ccw_view_fn {
    using result_type = Result;
    template <typename View> result_type operator()(const View& src) const { return result_type(rotated90ccw_view(src)); }
};
template <typename Result> struct tranposed_view_fn {
    using result_type = Result;
    template <typename View> result_type operator()(const View& src) const { return result_type(tranposed_view(src)); }
};
template <typename Result> struct rotated180_view_fn {
    using result_type = Result;
    template <typename View> result_type operator()(const View& src) const { return result_type(rotated180_view(src)); }
};

template <typename Result>
struct subimage_view_fn
{
    using result_type = Result;
    subimage_view_fn(point_t const& topleft, point_t const& dimensions)
        : _topleft(topleft), _size2(dimensions)
    {}

    template <typename View>
    result_type operator()(const View& src) const
    {
        return result_type(subimage_view(src,_topleft,_size2));
    }

    point_t _topleft;
    point_t _size2;
};

template <typename Result>
struct subsampled_view_fn
{
    using result_type = Result;
    subsampled_view_fn(point_t const& step) : _step(step) {}

    template <typename View>
    result_type operator()(const View& src) const
    {
        return result_type(subsampled_view(src,_step));
    }

    point_t _step;
};

template <typename Result> struct nth_channel_view_fn {
    using result_type = Result;
    nth_channel_view_fn(int n) : _n(n) {}
    int _n;
    template <typename View> result_type operator()(const View& src) const { return result_type(nth_channel_view(src,_n)); }
};
template <typename DstP, typename Result, typename CC = default_color_converter> struct color_converted_view_fn {
    using result_type = Result;
    color_converted_view_fn(CC cc = CC()): _cc(cc) {}

    template <typename View> result_type operator()(const View& src) const { return result_type(color_converted_view<DstP>(src, _cc)); }

    private:
        CC _cc;
};
} // namespace detail


/// \ingroup ImageViewTransformationsFlipUD
template <typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
typename dynamic_y_step_type<any_image_view<ViewTypes> >::type flipped_up_down_view(const any_image_view<ViewTypes>& src) {
    return apply_operation(src,detail::flipped_up_down_view_fn<typename dynamic_y_step_type<any_image_view<ViewTypes> >::type>());
}

/// \ingroup ImageViewTransformationsFlipLR
template <typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
typename dynamic_x_step_type<any_image_view<ViewTypes> >::type flipped_left_right_view(const any_image_view<ViewTypes>& src) {
    return apply_operation(src,detail::flipped_left_right_view_fn<typename dynamic_x_step_type<any_image_view<ViewTypes> >::type>());
}

/// \ingroup ImageViewTransformationsTransposed
template <typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
typename dynamic_xy_step_transposed_type<any_image_view<ViewTypes> >::type transposed_view(const any_image_view<ViewTypes>& src) {
    return apply_operation(src,detail::tranposed_view_fn<typename dynamic_xy_step_transposed_type<any_image_view<ViewTypes> >::type>());
}

/// \ingroup ImageViewTransformations90CW
template <typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
typename dynamic_xy_step_transposed_type<any_image_view<ViewTypes> >::type rotated90cw_view(const any_image_view<ViewTypes>& src) {
    return apply_operation(src,detail::rotated90cw_view_fn<typename dynamic_xy_step_transposed_type<any_image_view<ViewTypes> >::type>());
}

/// \ingroup ImageViewTransformations90CCW
template <typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
typename dynamic_xy_step_transposed_type<any_image_view<ViewTypes> >::type rotated90ccw_view(const any_image_view<ViewTypes>& src) {
    return apply_operation(src,detail::rotated90ccw_view_fn<typename dynamic_xy_step_transposed_type<any_image_view<ViewTypes> >::type>());
}

/// \ingroup ImageViewTransformations180
/// Models MPL Random Access Container of models of ImageViewConcept
template <typename ViewTypes>
inline auto rotated180_view(const any_image_view<ViewTypes>& src)
    -> typename dynamic_xy_step_type<any_image_view<ViewTypes>>::type
{
    using step_type = typename dynamic_xy_step_type<any_image_view<ViewTypes>>::type;
    return apply_operation(src, detail::rotated180_view_fn<step_type>());
}

/// \ingroup ImageViewTransformationsSubimage
///  // Models MPL Random Access Container of models of ImageViewConcept
template <typename ViewTypes>
inline auto subimage_view(any_image_view<ViewTypes> const& src,
                          point_t const& topleft, point_t const& dimensions)
    -> any_image_view<ViewTypes>
{
    using subimage_view_fn = detail::subimage_view_fn<any_image_view<ViewTypes>>;
    return apply_operation(src, subimage_view_fn(topleft, dimensions));
}

/// \ingroup ImageViewTransformationsSubimage
/// Models MPL Random Access Container of models of ImageViewConcept
template <typename ViewTypes>
inline auto subimage_view(any_image_view<ViewTypes> const& src,
                          int xMin, int yMin, int width, int height)
    -> any_image_view<ViewTypes>
{
    using subimage_view_fn = detail::subimage_view_fn<any_image_view<ViewTypes>>;
    return apply_operation(src, subimage_view_fn(point_t(xMin, yMin),point_t(width, height)));
}

/// \ingroup ImageViewTransformationsSubsampled
/// Models MPL Random Access Container of models of ImageViewConcept
template <typename ViewTypes>
inline auto subsampled_view(any_image_view<ViewTypes> const& src, point_t const& step)
    -> typename dynamic_xy_step_type<any_image_view<ViewTypes>>::type
{
    using step_type = typename dynamic_xy_step_type<any_image_view<ViewTypes>>::type;
    using subsampled_view = detail::subsampled_view_fn<step_type>;
    return apply_operation(src, subsampled_view(step));
}

/// \ingroup ImageViewTransformationsSubsampled
/// Models MPL Random Access Container of models of ImageViewConcept
template <typename ViewTypes>
inline auto subsampled_view(any_image_view<ViewTypes> const& src, int xStep, int yStep)
    -> typename dynamic_xy_step_type<any_image_view<ViewTypes>>::type
{
    using step_type = typename dynamic_xy_step_type<any_image_view<ViewTypes>>::type;
    using subsampled_view_fn = detail::subsampled_view_fn<step_type>;
    return apply_operation(src, subsampled_view_fn(point_t(xStep, yStep)));
}

namespace detail {
    template <typename View> struct get_nthchannel_type { using type = typename nth_channel_view_type<View>::type; };
    template <typename Views> struct views_get_nthchannel_type : public mpl::transform<Views, get_nthchannel_type<mpl::_1> > {};
}

/// \ingroup ImageViewTransformationsNthChannel
/// \brief Given a runtime source image view, returns the type of a runtime image view over a single channel of the source view
template <typename ViewTypes>
struct nth_channel_view_type<any_image_view<ViewTypes> > {
    using type = any_image_view<typename detail::views_get_nthchannel_type<ViewTypes>::type>;
};

/// \ingroup ImageViewTransformationsNthChannel
template <typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
typename nth_channel_view_type<any_image_view<ViewTypes> >::type nth_channel_view(const any_image_view<ViewTypes>& src, int n) {
    return apply_operation(src,detail::nth_channel_view_fn<typename nth_channel_view_type<any_image_view<ViewTypes> >::type>(n));
}

namespace detail {
    template <typename View, typename DstP, typename CC> struct get_ccv_type : public color_converted_view_type<View, DstP, CC> {};
    template <typename Views, typename DstP, typename CC> struct views_get_ccv_type : public mpl::transform<Views, get_ccv_type<mpl::_1,DstP,CC> > {};
}

/// \ingroup ImageViewTransformationsColorConvert
/// \brief Returns the type of a runtime-specified view, color-converted to a given pixel type with user specified color converter
template <typename ViewTypes, typename DstP, typename CC>
struct color_converted_view_type<any_image_view<ViewTypes>,DstP,CC>
{
    using type = any_image_view<typename detail::views_get_ccv_type<ViewTypes, DstP, CC>::type>;
};

/// \ingroup ImageViewTransformationsColorConvert
/// \brief overload of generic color_converted_view with user defined color-converter
template <typename DstP, typename ViewTypes, typename CC> inline // Models MPL Random Access Container of models of ImageViewConcept
typename color_converted_view_type<any_image_view<ViewTypes>, DstP, CC>::type color_converted_view(const any_image_view<ViewTypes>& src, CC) {
    return apply_operation(src,detail::color_converted_view_fn<DstP,typename color_converted_view_type<any_image_view<ViewTypes>, DstP, CC>::type >());
}

/// \ingroup ImageViewTransformationsColorConvert
/// \brief Returns the type of a runtime-specified view, color-converted to a given pixel type with the default coor converter
template <typename ViewTypes, typename DstP>
struct color_converted_view_type<any_image_view<ViewTypes>,DstP>
{
    using type = any_image_view<typename detail::views_get_ccv_type<ViewTypes, DstP, default_color_converter>::type>;
};

/// \ingroup ImageViewTransformationsColorConvert
/// \brief overload of generic color_converted_view with the default color-converter
template <typename DstP, typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
typename color_converted_view_type<any_image_view<ViewTypes>, DstP>::type color_converted_view(const any_image_view<ViewTypes>& src) {
    return apply_operation(src,detail::color_converted_view_fn<DstP,typename color_converted_view_type<any_image_view<ViewTypes>, DstP>::type >());
}


/// \ingroup ImageViewTransformationsColorConvert
/// \brief overload of generic color_converted_view with user defined color-converter
///        These are workarounds for GCC 3.4, which thinks color_converted_view is ambiguous with the same method for templated views (in gil/image_view_factory.hpp)
template <typename DstP, typename ViewTypes, typename CC> inline // Models MPL Random Access Container of models of ImageViewConcept
typename color_converted_view_type<any_image_view<ViewTypes>, DstP, CC>::type any_color_converted_view(const any_image_view<ViewTypes>& src, CC) {
    return apply_operation(src,detail::color_converted_view_fn<DstP,typename color_converted_view_type<any_image_view<ViewTypes>, DstP, CC>::type >());
}

/// \ingroup ImageViewTransformationsColorConvert
/// \brief overload of generic color_converted_view with the default color-converter
///        These are workarounds for GCC 3.4, which thinks color_converted_view is ambiguous with the same method for templated views (in gil/image_view_factory.hpp)
template <typename DstP, typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
typename color_converted_view_type<any_image_view<ViewTypes>, DstP>::type any_color_converted_view(const any_image_view<ViewTypes>& src) {
    return apply_operation(src,detail::color_converted_view_fn<DstP,typename color_converted_view_type<any_image_view<ViewTypes>, DstP>::type >());
}

/// \}

} }  // namespace boost::gil

#endif
