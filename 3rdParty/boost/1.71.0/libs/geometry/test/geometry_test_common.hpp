// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef GEOMETRY_TEST_GEOMETRY_TEST_COMMON_HPP
#define GEOMETRY_TEST_GEOMETRY_TEST_COMMON_HPP

#include <boost/config.hpp>

// Determine debug/release mode
// (it would be convenient if Boost.Config or Boost.Test would define this)
// Note that they might be combined (e.g. for optimize+no inline)
#if defined (BOOST_CLANG) || defined(BOOST_GCC)
#if defined(__OPTIMIZE__)
    #define BOOST_GEOMETRY_COMPILER_MODE_RELEASE
#endif
#if defined(__NO_INLINE__)
#define BOOST_GEOMETRY_COMPILER_MODE_DEBUG
#endif
#endif

#if defined(BOOST_MSVC)
#if defined(_DEBUG)
#define BOOST_GEOMETRY_COMPILER_MODE_DEBUG
#else
#define BOOST_GEOMETRY_COMPILER_MODE_RELEASE
#endif
#endif


#if defined(BOOST_MSVC)
// We deliberately mix float/double's  so turn off warnings
#pragma warning( disable : 4244 )
// For (new since Boost 1.40) warning in Boost.Test on putenv/posix
#pragma warning( disable : 4996 )

//#pragma warning( disable : 4305 )
#endif // defined(BOOST_MSVC)

#include <boost/config.hpp>
#include <boost/concept_check.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/foreach.hpp>

#include <string_from_type.hpp>

// Include some always-included-for-testing files
#if ! defined(BOOST_GEOMETRY_NO_BOOST_TEST)

// Until Boost.Test fixes it, silence warning issued by clang:
#ifdef __clang__
# pragma clang diagnostic push
// warning: unused variable 'check_is_close' [-Wunused-variable]
# pragma clang diagnostic ignored "-Wunused-variable"
// warnings when -Wconversion is set
# pragma clang diagnostic ignored "-Wsign-conversion"
# pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif

# include <boost/test/floating_point_comparison.hpp>
#ifndef BOOST_TEST_MODULE
# include <boost/test/included/test_exec_monitor.hpp>
//#  include <boost/test/included/prg_exec_monitor.hpp>
# include <boost/test/impl/execution_monitor.ipp>
#endif

#ifdef __clang__
# pragma clang diagnostic pop
#endif

#endif


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

// For all tests:
// - do NOT use "using namespace boost::geometry" to make clear what is Boost.Geometry
// - use bg:: as short alias
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/config.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/tag.hpp>
namespace bg = boost::geometry;


template <typename CoordinateType, typename T1, typename T2>
inline T1 if_typed_tt(T1 value_tt, T2 value)
{
#if defined(HAVE_TTMATH)
    return boost::is_same<CoordinateType, ttmath_big>::type::value ? value_tt : value;
#else
    boost::ignore_unused(value_tt);
    return value;
#endif
}

template <typename CoordinateType, typename Specified, typename T>
inline T if_typed(T value_typed, T value)
{
    return boost::is_same<CoordinateType, Specified>::value ? value_typed : value;
}

template <typename Geometry1, typename Geometry2>
inline std::string type_for_assert_message()
{
    bool const ccw =
        bg::point_order<Geometry1>::value == bg::counterclockwise
        || bg::point_order<Geometry2>::value == bg::counterclockwise;
    bool const open =
        bg::closure<Geometry1>::value == bg::open
        || bg::closure<Geometry2>::value == bg::open;

    std::ostringstream out;
    out << string_from_type<typename bg::coordinate_type<Geometry1>::type>::name()
        << (ccw ? " ccw" : "")
        << (open ? " open" : "");
    return out.str();
}

struct geographic_policy
{
    template <typename CoordinateType>
    static inline CoordinateType apply(CoordinateType const& value)
    {
        return value;
    }
};

struct mathematical_policy
{
    template <typename CoordinateType>
    static inline CoordinateType apply(CoordinateType const& value)
    {
        return 90 - value;
    }

};

#if defined(BOOST_GEOMETRY_USE_RESCALING)
#define BG_IF_RESCALED(a, b) a
#else
#define BG_IF_RESCALED(a, b) b
#endif

#endif // GEOMETRY_TEST_GEOMETRY_TEST_COMMON_HPP
