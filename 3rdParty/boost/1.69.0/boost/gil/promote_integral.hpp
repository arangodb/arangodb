// Boost.GIL (Generic Image Library)
//
// Copyright (c) 2015, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
//
// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html
//
// Source: Boost.Geometry (aka GGL, Generic Geometry Library)
// Modifications: adapted for Boost.GIL
//  - Rename namespace boost::geometry to boost::gil
//  - Rename include guards
//  - Remove support for boost::multiprecision types
//  - Remove support for 128-bit integer types
//
#ifndef BOOST_GIL_PROMOTE_INTEGRAL_HPP
#define BOOST_GIL_PROMOTE_INTEGRAL_HPP

#include <boost/mpl/begin.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/end.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/next.hpp>

#include <climits>
#include <cstddef>
#include <type_traits>

namespace boost { namespace gil
{

namespace detail { namespace promote_integral
{

// meta-function that returns the bit size of a type
template
<
    typename T,
    bool IsFundamental = std::is_fundamental<T>::type::value
>
struct bit_size {};

// for fundamental types, just return CHAR_BIT * sizeof(T)
template <typename T>
struct bit_size<T, true> : std::integral_constant<std::size_t, (CHAR_BIT * sizeof(T))> {};

template
<
    typename T,
    typename Iterator,
    typename EndIterator,
    std::size_t MinSize
>
struct promote_to_larger
{
    typedef typename boost::mpl::deref<Iterator>::type current_type;

    typedef typename std::conditional
        <
            (bit_size<current_type>::type::value >= MinSize),
            current_type,
            typename promote_to_larger
                <
                    T,
                    typename boost::mpl::next<Iterator>::type,
                    EndIterator,
                    MinSize
                >::type
        >::type type;
};

// The following specialization is required to finish the loop over
// all list elements
template <typename T, typename EndIterator, std::size_t MinSize>
struct promote_to_larger<T, EndIterator, EndIterator, MinSize>
{
    // if promotion fails, keep the number T
    // (and cross fingers that overflow will not occur)
    typedef T type;
};

}} // namespace detail::promote_integral

/*!
    \brief Meta-function to define an integral type with size
    than is (roughly) twice the bit size of T
    \ingroup utility
    \details
    This meta-function tries to promote the fundamental integral type T
    to a another integral type with size (roughly) twice the bit size of T.

    To do this, two times the bit size of T is tested against the bit sizes of:
         short, int, long, boost::long_long_type, boost::int128_t
    and the one that first matches is chosen.

    For unsigned types the bit size of T is tested against the bit
    sizes of the types above, if T is promoted to a signed type, or
    the bit sizes of
         unsigned short, unsigned int, unsigned long, std::size_t,
         boost::ulong_long_type, boost::uint128_t
    if T is promoted to an unsigned type.

    By default an unsigned type is promoted to a signed type.
    This behavior is controlled by the PromoteUnsignedToUnsigned
    boolean template parameter, whose default value is "false".
    To promote an unsigned type to an unsigned type set the value of
    this template parameter to "true".

    Finally, if the passed type is either a floating-point type or a
    user-defined type it is returned as is.

    \note boost::long_long_type and boost::ulong_long_type are
    considered only if the macro BOOST_HAS_LONG_LONG is defined

*/
template
<
    typename T,
    bool PromoteUnsignedToUnsigned = false,
    bool UseCheckedInteger = false,
    bool IsIntegral = std::is_integral<T>::type::value
>
class promote_integral
{
private:
    static bool const is_unsigned = std::is_unsigned<T>::type::value;

    typedef detail::promote_integral::bit_size<T> bit_size_type;

    // Define the minimum size (in bits) needed for the promoted type
    // If T is the input type and P the promoted type, then the
    // minimum number of bits for P are (below b stands for the number
    // of bits of T):
    // * if T is unsigned and P is unsigned: 2 * b
    // * if T is signed and P is signed: 2 * b - 1
    // * if T is unsigned and P is signed: 2 * b + 1
    typedef typename std::conditional
        <
            (PromoteUnsignedToUnsigned && is_unsigned),
            std::integral_constant<std::size_t, (2 * bit_size_type::value)>,
            typename std::conditional
                <
                    is_unsigned,
                    std::integral_constant<std::size_t, (2 * bit_size_type::value + 1)>,
                    std::integral_constant<std::size_t, (2 * bit_size_type::value - 1)>
                >::type
        >::type min_bit_size_type;

    // Define the list of signed integral types we are going to use
    // for promotion
    typedef boost::mpl::list
        <
            short, int, long
#if defined(BOOST_HAS_LONG_LONG)
            , boost::long_long_type
#endif
        > signed_integral_types;

    // Define the list of unsigned integral types we are going to use
    // for promotion
    typedef boost::mpl::list
        <
            unsigned short, unsigned int, unsigned long, std::size_t
#if defined(BOOST_HAS_LONG_LONG)
            , boost::ulong_long_type
#endif
        > unsigned_integral_types;

    // Define the list of integral types that will be used for
    // promotion (depending in whether we was to promote unsigned to
    // unsigned or not)
    typedef typename std::conditional
        <
            (is_unsigned && PromoteUnsignedToUnsigned),
            unsigned_integral_types,
            signed_integral_types
        >::type integral_types;

public:
    typedef typename detail::promote_integral::promote_to_larger
        <
            T,
            typename boost::mpl::begin<integral_types>::type,
            typename boost::mpl::end<integral_types>::type,
            min_bit_size_type::value
        >::type type;
};


template <typename T, bool PromoteUnsignedToUnsigned, bool UseCheckedInteger>
class promote_integral
    <
        T, PromoteUnsignedToUnsigned, UseCheckedInteger, false
    >
{
public:
    typedef T type;
};

}} // namespace boost::gil

#endif // BOOST_GIL_PROMOTE_INTEGRAL_HPP
