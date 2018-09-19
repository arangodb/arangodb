/*
    Copyright 2005-2007 Adobe Systems Incorporated
   
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_NUMERIC_PIXEL_NUMERIC_OPERATIONS_HPP
#define BOOST_GIL_EXTENSION_NUMERIC_PIXEL_NUMERIC_OPERATIONS_HPP

/*!
/// \file               
/// \brief Structures for pixel-wise numeric operations
/// \author Lubomir Bourdev and Hailin Jin \n
///         Adobe Systems Incorporated
/// \date   2005-2007 \n
/// Currently defined structures:
///     pixel_plus_t (+), pixel_minus_t (-)
///     pixel_multiplies_scalar_t (*), pixel_divides_scalar_t (/)
///     pixel_halves_t (/=2), pixel_zeros_t (=0)
///     pixel_assigns_t (=)
*/

#include <functional>

#include <boost/gil/gil_config.hpp>
#include <boost/gil/pixel.hpp>
#include <boost/gil/color_base_algorithm.hpp>

#include <boost/gil/extension/numeric/channel_numeric_operations.hpp>

namespace boost { namespace gil {

/// \ingroup PixelNumericOperations
/// \brief construct for adding two pixels
template <typename PixelRef1, // models pixel concept
          typename PixelRef2, // models pixel concept
          typename PixelR>    // models pixel value concept
struct pixel_plus_t {
    PixelR operator() (const PixelRef1& p1,
                       const PixelRef2& p2) const {
        PixelR result;
        static_transform(p1,p2,result,
                           channel_plus_t<typename channel_type<PixelRef1>::type,
                                          typename channel_type<PixelRef2>::type,
                                          typename channel_type<PixelR>::type>());
        return result;
    }
};

/// \ingroup PixelNumericOperations
/// \brief construct for subtracting two pixels
template <typename PixelRef1, // models pixel concept
          typename PixelRef2, // models pixel concept
          typename PixelR>    // models pixel value concept
struct pixel_minus_t {
    PixelR operator() (const PixelRef1& p1,
                       const PixelRef2& p2) const {
        PixelR result;
        static_transform(p1,p2,result,
                           channel_minus_t<typename channel_type<PixelRef1>::type,
                                           typename channel_type<PixelRef2>::type,
                                           typename channel_type<PixelR>::type>());
        return result;
    }
};

/// \ingroup PixelNumericOperations
/// \brief construct for multiplying scalar to a pixel
template <typename PixelRef, // models pixel concept
          typename Scalar,   // models a scalar type
          typename PixelR>   // models pixel value concept
struct pixel_multiplies_scalar_t {
    PixelR operator () (const PixelRef& p,
                        const Scalar& s) const {
        PixelR result;
        static_transform(p,result,
                           std::bind2nd(channel_multiplies_scalar_t<typename channel_type<PixelRef>::type,
                                                                    Scalar,
                                                                    typename channel_type<PixelR>::type>(),s));
        return result;
    }
};

/// \ingroup PixelNumericOperations
/// \brief construct for dividing two pixels
template <typename PixelRef1, // models pixel concept
          typename PixelRef2, // models pixel concept
          typename PixelR>    // models pixel value concept
struct pixel_multiply_t {
    PixelR operator() (const PixelRef1& p1,
                       const PixelRef2& p2) const {
        PixelR result;
        static_transform(p1,p2,result,
                           channel_multiplies_t<typename channel_type<PixelRef1>::type,
                                                typename channel_type<PixelRef2>::type,
                                                typename channel_type<PixelR>::type>());
        return result;
    }
};

/// \ingroup PixelNumericOperations
/// \brief construct for dividing a pixel by a scalar
template <typename PixelRef, // models pixel concept
          typename Scalar,   // models a scalar type
          typename PixelR>   // models pixel value concept
struct pixel_divides_scalar_t {
    PixelR operator () (const PixelRef& p,
                        const Scalar& s) const {
        PixelR result;
        static_transform(p,result,
                           std::bind2nd(channel_divides_scalar_t<typename channel_type<PixelRef>::type,
                                                                 Scalar,
                                                                 typename channel_type<PixelR>::type>(),s));
        return result;
    }
};

/// \ingroup PixelNumericOperations
/// \brief construct for dividing two pixels
template <typename PixelRef1, // models pixel concept
          typename PixelRef2, // models pixel concept
          typename PixelR>    // models pixel value concept
struct pixel_divide_t {
    PixelR operator() (const PixelRef1& p1,
                       const PixelRef2& p2) const {
        PixelR result;
        static_transform(p1,p2,result,
                           channel_divides_t<typename channel_type<PixelRef1>::type,
                                             typename channel_type<PixelRef2>::type,
                                             typename channel_type<PixelR>::type>());
        return result;
    }
};

/// \ingroup PixelNumericOperations
/// \brief construct for dividing a pixel by 2
template <typename PixelRef> // models pixel concept
struct pixel_halves_t {
    PixelRef& operator () (PixelRef& p) const {
        static_for_each(p,channel_halves_t<typename channel_type<PixelRef>::type>());
        return p;
    }
};

/// \ingroup PixelNumericOperations
/// \brief construct for setting a pixel to zero (for whatever zero means)
template <typename PixelRef> // models pixel concept
struct pixel_zeros_t {
    PixelRef& operator () (PixelRef& p) const {
        static_for_each(p,channel_zeros_t<typename channel_type<PixelRef>::type>());
        return p;
    }
};

// Hailin: This is how you can do it:
template <typename Pixel>
void zero_channels(Pixel& p) {
    static_for_each(p,channel_zeros_t<typename channel_type<Pixel>::type>());
}


/// \ingroup PixelNumericOperations
///definition and a generic implementation for casting and assigning a pixel to another
///user should specialize it for better performance
template <typename PixelRef,  // models pixel concept
          typename PixelRefR> // models pixel concept
struct pixel_assigns_t {
    PixelRefR operator () (const PixelRef& src,
                           PixelRefR& dst) const {
        static_for_each(src,dst,channel_assigns_t<typename channel_type<PixelRef>::type,
                                                  typename channel_type<PixelRefR>::type>());
        return dst;
    }
};

} }  // namespace boost::gil

#endif // BOOST_GIL_EXTENSION_NUMERIC_PIXEL_NUMERIC_OPERATIONS_HPP
