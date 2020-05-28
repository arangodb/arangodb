//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_CONCEPTS_COLOR_HPP
#define BOOST_GIL_CONCEPTS_COLOR_HPP

#include <boost/gil/concepts/concept_check.hpp>

#include <boost/type_traits.hpp>

#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedefs"
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

namespace boost { namespace gil {

/// \ingroup ColorSpaceAndLayoutConcept
/// \brief Color space type concept
/// \code
/// concept ColorSpaceConcept<MPLRandomAccessSequence CS>
/// {
///    // An MPL Random Access Sequence, whose elements are color tags
/// };
/// \endcode
template <typename CS>
struct ColorSpaceConcept
{
    void constraints()
    {
        // An MPL Random Access Sequence, whose elements are color tags

        // TODO: Is this incomplete?
    }
};

// Models ColorSpaceConcept
template <typename CS1, typename CS2>
struct color_spaces_are_compatible : is_same<CS1, CS2>
{
};

/// \ingroup ColorSpaceAndLayoutConcept
/// \brief Two color spaces are compatible if they are the same
/// \code
/// concept ColorSpacesCompatibleConcept<ColorSpaceConcept CS1, ColorSpaceConcept CS2>
/// {
///     where SameType<CS1, CS2>;
/// };
/// \endcode
template <typename CS1, typename CS2>
struct ColorSpacesCompatibleConcept
{
    void constraints()
    {
        static_assert(color_spaces_are_compatible<CS1, CS2>::value, "");
    }
};

/// \ingroup ColorSpaceAndLayoutConcept
/// \brief Channel mapping concept
/// \code
/// concept ChannelMappingConcept<MPLRandomAccessSequence CM>
/// {
///     // An MPL Random Access Sequence, whose elements
///     // model MPLIntegralConstant representing a permutation
/// };
/// \endcode
template <typename CM>
struct ChannelMappingConcept
{
    void constraints()
    {
        // An MPL Random Access Sequence, whose elements model
        // MPLIntegralConstant representing a permutation.

        // TODO: Is this incomplete?
    }
};

}} // namespace boost::gil

#if defined(BOOST_CLANG)
#pragma clang diagnostic pop
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic pop
#endif

#endif
