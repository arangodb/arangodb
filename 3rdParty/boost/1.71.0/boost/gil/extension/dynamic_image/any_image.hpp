//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_DYNAMIC_IMAGE_ANY_IMAGE_HPP
#define BOOST_GIL_EXTENSION_DYNAMIC_IMAGE_ANY_IMAGE_HPP

#include <boost/gil/extension/dynamic_image/any_image_view.hpp>
#include <boost/gil/extension/dynamic_image/apply_operation.hpp>

#include <boost/gil/image.hpp>

#include <boost/config.hpp>

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(push)
#pragma warning(disable:4512) //assignment operator could not be generated
#endif

namespace boost { namespace gil {

namespace detail {
    template <typename T> struct get_view_t       { using type = typename T::view_t; };
    template <typename Images> struct images_get_views_t : public mpl::transform<Images, get_view_t<mpl::_1> > {};

    template <typename T> struct get_const_view_t { using type = typename T::const_view_t; };
    template <typename Images> struct images_get_const_views_t : public mpl::transform<Images, get_const_view_t<mpl::_1> > {};

    struct recreate_image_fnobj
    {
        using result_type = void;
        point<std::ptrdiff_t> const& _dimensions;
        unsigned _alignment;

        recreate_image_fnobj(point<std::ptrdiff_t> const& dims, unsigned alignment) : _dimensions(dims), _alignment(alignment) {}
        template <typename Image>
        result_type operator()(Image& img) const { img.recreate(_dimensions,_alignment); }
    };

    template <typename AnyView>  // Models AnyViewConcept
    struct any_image_get_view {
        using result_type = AnyView;
        template <typename Image> result_type operator()(      Image& img) const { return result_type(view(img)); }
    };

    template <typename AnyConstView>  // Models AnyConstViewConcept
    struct any_image_get_const_view {
        using result_type = AnyConstView;
        template <typename Image> result_type operator()(const Image& img) const { return result_type(const_view(img)); }
    };
}

////////////////////////////////////////////////////////////////////////////////////////
/// \ingroup ImageModel
/// \brief Represents a run-time specified image. Note it does NOT model ImageConcept
///
/// Represents an image whose type (color space, layout, planar/interleaved organization, etc) can be specified at run time.
/// It is the runtime equivalent of \p image.
/// Some of the requirements of ImageConcept, such as the \p value_type alias cannot be fulfilled, since the language does not allow runtime type specification.
/// Other requirements, such as access to the pixels, would be inefficient to provide. Thus \p any_image does not fully model ImageConcept.
/// In particular, its \p view and \p const_view methods return \p any_image_view, which does not fully model ImageViewConcept. See \p any_image_view for more.
////////////////////////////////////////////////////////////////////////////////////////
template <typename ImageTypes>
class any_image : public make_variant_over<ImageTypes>::type {
    using parent_t = typename make_variant_over<ImageTypes>::type;
public:
    using const_view_t = any_image_view<typename detail::images_get_const_views_t<ImageTypes>::type>;
    using view_t = any_image_view<typename detail::images_get_views_t<ImageTypes>::type>;
    using x_coord_t = std::ptrdiff_t;
    using y_coord_t = std::ptrdiff_t;
    using point_t = point<std::ptrdiff_t>;

    any_image()                                                          : parent_t() {}
    template <typename T> explicit any_image(const T& obj)               : parent_t(obj) {}
    template <typename T> explicit any_image(T& obj, bool do_swap)       : parent_t(obj,do_swap) {}
    any_image(const any_image& v)                                        : parent_t((const parent_t&)v)    {}
    template <typename Types> any_image(const any_image<Types>& v)       : parent_t((const typename make_variant_over<Types>::type&)v)    {}

    template <typename T> any_image& operator=(const T& obj)                  { parent_t::operator=(obj); return *this; }
    any_image&                       operator=(const any_image& v)            { parent_t::operator=((const parent_t&)v); return *this;}
    template <typename Types> any_image& operator=(const any_image<Types>& v) { parent_t::operator=((const typename make_variant_over<Types>::type&)v); return *this;}

    void recreate(const point_t& dims, unsigned alignment=1)
    {
        apply_operation(*this,detail::recreate_image_fnobj(dims,alignment));
    }

    void recreate(x_coord_t width, y_coord_t height, unsigned alignment=1)
    {
        recreate({width, height}, alignment);
    }

    std::size_t num_channels()  const { return apply_operation(*this, detail::any_type_get_num_channels()); }
    point_t     dimensions()    const { return apply_operation(*this, detail::any_type_get_dimensions()); }
    x_coord_t   width()         const { return dimensions().x; }
    y_coord_t   height()        const { return dimensions().y; }
};

///@{
/// \name view, const_view
/// \brief Get an image view from a run-time instantiated image

/// \ingroup ImageModel

/// \brief Returns the non-constant-pixel view of any image. The returned view is any view.
template <typename Types>  BOOST_FORCEINLINE // Models ImageVectorConcept
typename any_image<Types>::view_t view(any_image<Types>& anyImage) {
    return apply_operation(anyImage, detail::any_image_get_view<typename any_image<Types>::view_t>());
}

/// \brief Returns the constant-pixel view of any image. The returned view is any view.
template <typename Types> BOOST_FORCEINLINE // Models ImageVectorConcept
typename any_image<Types>::const_view_t const_view(const any_image<Types>& anyImage) {
    return apply_operation(anyImage, detail::any_image_get_const_view<typename any_image<Types>::const_view_t>());
}
///@}

}}  // namespace boost::gil

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(pop)
#endif

#endif
