// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2015.
// Modifications copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef GEOMETRY_TEST_STRING_FROM_TYPE_HPP
#define GEOMETRY_TEST_STRING_FROM_TYPE_HPP

#include <string>

#if defined(HAVE_TTMATH)
#  include <boost/geometry/extensions/contrib/ttmath_stub.hpp>
#endif

#if defined(HAVE_CLN) || defined(HAVE_GMP)
#  include <boost/numeric_adaptor/numeric_adaptor.hpp>
#endif


#if defined(HAVE_GMP)
#  include <boost/numeric_adaptor/gmp_value_type.hpp>
#endif
#if defined(HAVE_CLN)
#  include <boost/numeric_adaptor/cln_value_type.hpp>
#endif



template <typename T>
struct string_from_type {};

template <> struct string_from_type<void>
{ static std::string name() { return "v"; }  };

template <> struct string_from_type<float>
{ static std::string name() { return "f"; }  };

template <> struct string_from_type<double>
{ static std::string name() { return "d"; }  };

template <> struct string_from_type<long double>
{ static std::string name() { return "e"; }  };

template <> struct string_from_type<short int>
{ static std::string name() { return "s"; }  };

template <> struct string_from_type<int>
{ static std::string name() { return "i"; }  };

template <> struct string_from_type<long>
{ static std::string name() { return "l"; }  };

#if defined(BOOST_HAS_LONG_LONG)
// this is what g++ and clang++ use
template <> struct string_from_type<boost::long_long_type>
{ static std::string name() { return "x"; }  };
#endif

#if defined(BOOST_HAS_INT128)
// this is what g++ and clang++ use
template <> struct string_from_type<boost::int128_type>
{ static std::string name() { return "n"; }  };
#endif

#if defined(HAVE_TTMATH)
    template <> struct string_from_type<ttmath_big>
    { static std::string name() { return "t"; }  };
#endif

#if defined(BOOST_RATIONAL_HPP)
template <typename T> struct string_from_type<boost::rational<T> >
{ static std::string name() { return "r"; }  };
#endif


#if defined(HAVE_GMP)
template <> struct string_from_type<boost::numeric_adaptor::gmp_value_type>
{ static std::string name() { return "g"; }  };
#endif

#if defined(HAVE_CLN)
template <> struct string_from_type<boost::numeric_adaptor::cln_value_type>
{ static std::string name() { return "c"; }  };
#endif


#endif // GEOMETRY_TEST_STRING_FROM_TYPE_HPP
