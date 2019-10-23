#ifndef BOOST_ENDIAN_DETAIL_ORDER_HPP_INCLUDED
#define BOOST_ENDIAN_DETAIL_ORDER_HPP_INCLUDED

// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/core/scoped_enum.hpp>
#include <boost/predef/other/endian.h>

namespace boost
{
namespace endian
{

BOOST_SCOPED_ENUM_START(order)
{
    big, little,

# if BOOST_ENDIAN_BIG_BYTE

    native = big

# else

    native = little

# endif
}; BOOST_SCOPED_ENUM_END

} // namespace endian
} // namespace boost

#endif  // BOOST_ENDIAN_DETAIL_ORDER_HPP_INCLUDED
