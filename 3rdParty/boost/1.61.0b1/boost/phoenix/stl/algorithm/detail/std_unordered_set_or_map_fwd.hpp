/////////////////////////////////////////////////////////////////////////////
// Copyright 2005 Daniel Wallin.
// Copyright 2005 Joel de Guzman.
// Copyright 2015 John Fletcher
//
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// Modeled after range_ex, Copyright 2004 Eric Niebler
///////////////////////////////////////////////////////////////////////////////
//
// std_unordered_set_or_map_fwd.hpp
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_PHOENIX_STD_UNORDERED_SET_OR_MAP_FWD
#define BOOST_PHOENIX_STD_UNORDERED_SET_OR_MAP_FWD

#include <boost/phoenix/config.hpp>

#ifdef BOOST_PHOENIX_HAS_UNORDERED_SET_AND_MAP
#ifdef BOOST_PHOENIX_USING_LIBCPP
// Advance declaration not working for libc++
#include <unordered_set>
#include <unordered_map>
#else



namespace std {


    template<
        class Kty
      , class Hash
      , class Cmp
      , class Alloc
    >
    class unordered_set;

    template<
        class Kty
      , class Hash
      , class Cmp
      , class Alloc
    >
    class unordered_multiset;

    template<
        class Kty
      , class Ty
      , class Hash
      , class Cmp
      , class Alloc
    >
    class unordered_map;

    template<
        class Kty
      , class Ty
      , class Hash
      , class Cmp
      , class Alloc
    >
    class unordered_multimap;
}
#endif


#endif

#endif
