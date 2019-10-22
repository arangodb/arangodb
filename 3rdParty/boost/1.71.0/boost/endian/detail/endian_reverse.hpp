#ifndef BOOST_ENDIAN_DETAIL_ENDIAN_REVERSE_HPP_INCLUDED
#define BOOST_ENDIAN_DETAIL_ENDIAN_REVERSE_HPP_INCLUDED

// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/detail/integral_by_size.hpp>
#include <boost/endian/detail/intrinsic.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/static_assert.hpp>
#include <boost/cstdint.hpp>
#include <boost/config.hpp>
#include <cstddef>
#include <cstring>

namespace boost
{
namespace endian
{

namespace detail
{

//  -- portable approach suggested by tymofey, with avoidance of undefined behavior
//     as suggested by Giovanni Piero Deretta, with a further refinement suggested
//     by Pyry Jahkola.
//  -- intrinsic approach suggested by reviewers, and by David Stone, who provided
//     his Boost licensed macro implementation (detail/intrinsic.hpp)

inline uint8_t endian_reverse_impl( uint8_t x ) BOOST_NOEXCEPT
{
    return x;
}

inline uint16_t endian_reverse_impl( uint16_t x ) BOOST_NOEXCEPT
{
#ifdef BOOST_ENDIAN_NO_INTRINSICS

    return (x << 8) | (x >> 8);

#else

    return BOOST_ENDIAN_INTRINSIC_BYTE_SWAP_2(x);

#endif
}

inline uint32_t endian_reverse_impl(uint32_t x) BOOST_NOEXCEPT
{
#ifdef BOOST_ENDIAN_NO_INTRINSICS

    uint32_t step16 = x << 16 | x >> 16;
    return ((step16 << 8) & 0xff00ff00) | ((step16 >> 8) & 0x00ff00ff);

#else

    return BOOST_ENDIAN_INTRINSIC_BYTE_SWAP_4(x);

#endif
}

inline uint64_t endian_reverse_impl(uint64_t x) BOOST_NOEXCEPT
{
#ifdef BOOST_ENDIAN_NO_INTRINSICS

    uint64_t step32 = x << 32 | x >> 32;
    uint64_t step16 = (step32 & 0x0000FFFF0000FFFFULL) << 16 | (step32 & 0xFFFF0000FFFF0000ULL) >> 16;
    return (step16 & 0x00FF00FF00FF00FFULL) << 8 | (step16 & 0xFF00FF00FF00FF00ULL) >> 8;

#else

    return BOOST_ENDIAN_INTRINSIC_BYTE_SWAP_8(x);

# endif
}

} // namespace detail

// Requires:
//    T is non-bool integral

template<class T> inline T endian_reverse( T x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( is_integral<T>::value && !is_same<T, bool>::value );

    typedef typename detail::integral_by_size< sizeof(T) >::type uintN_t;

    return static_cast<T>( detail::endian_reverse_impl( static_cast<uintN_t>( x ) ) );
}

template <class EndianReversible>
inline void endian_reverse_inplace(EndianReversible& x) BOOST_NOEXCEPT
{
    x = endian_reverse( x );
}

} // namespace endian
} // namespace boost

#endif  // BOOST_ENDIAN_DETAIL_ENDIAN_REVERSE_HPP_INCLUDED
