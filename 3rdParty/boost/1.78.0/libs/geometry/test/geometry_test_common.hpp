// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

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

# include <boost/test/tools/floating_point_comparison.hpp>
#ifndef BOOST_TEST_MODULE
# include <boost/test/included/test_exec_monitor.hpp>
//#  include <boost/test/included/prg_exec_monitor.hpp>
# include <boost/test/impl/execution_monitor.ipp>
#endif

#ifdef __clang__
# pragma clang diagnostic pop
#endif

#endif

// For testing high precision numbers
#include <boost/multiprecision/cpp_bin_float.hpp>

// For all tests:
// - do NOT use "using namespace boost::geometry" to make clear what is Boost.Geometry
// - use bg:: as short alias
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/config.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/tag.hpp>
namespace bg = boost::geometry;


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

struct ut_base_settings
{
    explicit ut_base_settings(bool validity = true)
        : m_test_validity(true)
    {
        set_test_validity(validity);
    }

    inline void set_test_validity(bool validity)
    {
#if defined(BOOST_GEOMETRY_TEST_FAILURES)
        boost::ignore_unused(validity);
#else
        m_test_validity = validity;
#endif
    }

    inline bool test_validity() const
    {
        return m_test_validity;
    }

private :
    bool m_test_validity;
};

//! Type used for tests using high precision numbers
using mp_test_type = boost::multiprecision::cpp_bin_float_100;

//! Default type for tests, can optionally be specified on the command line
//! e.g. float, "long double", mp_test_type
#if defined(BOOST_GEOMETRY_DEFAULT_TEST_TYPE)
using default_test_type = BOOST_GEOMETRY_DEFAULT_TEST_TYPE;
#else
using default_test_type = double;
#endif

//! Compile time function for expectations depending on type
template <typename CoordinateType, typename Specified, typename T>
inline T if_typed(T value_typed, T value)
{
    return std::is_same<CoordinateType, Specified>::value ? value_typed : value;
}

//! Compile time function for expectations depending on high precision
template <typename CoordinateType, typename T1, typename T2>
inline T1 const& bg_if_mp(T1 const& value_mp, T2 const& value)
{
    return std::is_same<CoordinateType, mp_test_type>::type::value ? value_mp : value;
}

//! Macro for expectations depending on rescaling
#if defined(BOOST_GEOMETRY_USE_RESCALING)
#define BG_IF_RESCALED(a, b) a
#else
#define BG_IF_RESCALED(a, b) b
#endif

//! Macro for turning of a test setting when testing without failures
#if defined(BOOST_GEOMETRY_TEST_FAILURES)
#define BG_IF_TEST_FAILURES true
#else
#define BG_IF_TEST_FAILURES false
#endif

inline void BoostGeometryWriteTestConfiguration()
{
    std::cout << std::endl << "Test configuration:" << std::endl;
#if defined(BOOST_GEOMETRY_COMPILER_MODE_RELEASE)
    std::cout << "  - Release mode" << std::endl;
#endif
#if defined(BOOST_GEOMETRY_COMPILER_MODE_DEBUG)
    std::cout << "  - Debug mode" << std::endl;
#endif
#if defined(BOOST_GEOMETRY_ROBUSTNESS_ALTERNATIVE)
    std::cout << "  - Flipping the robustness alternative" << std::endl;
#endif
#if defined(BOOST_GEOMETRY_USE_RESCALING)
    std::cout << "  - Using rescaling" << std::endl;
#else
    std::cout << "  - No rescaling" << std::endl;
#endif
#if defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    std::cout << "  - Testing only one type" << std::endl;
#endif
#if defined(BOOST_GEOMETRY_TEST_ONLY_ONE_ORDER)
    std::cout << "  - Testing only one order" << std::endl;
#endif
#if defined(BOOST_GEOMETRY_TEST_FAILURES)
    std::cout << "  - Including failing test cases" << std::endl;
#endif
    std::cout << "  - Default test type: " << string_from_type<default_test_type>::name() << std::endl;
    std::cout << std::endl;
}

#ifdef BOOST_GEOMETRY_TEST_FAILURES
#define BG_NO_FAILURES 0
inline void BoostGeometryWriteExpectedFailures(std::size_t for_rescaling,
                                               std::size_t for_no_rescaling_double,
                                               std::size_t for_no_rescaling_float,
                                               std::size_t for_no_rescaling_extended)
{
    std::size_t const for_no_rescaling
        = if_typed<default_test_type, double>(for_no_rescaling_double,
              if_typed<default_test_type, float>(for_no_rescaling_float,
                  for_no_rescaling_extended));

    boost::ignore_unused(for_rescaling, for_no_rescaling, for_no_rescaling_double,
                         for_no_rescaling_float, for_no_rescaling_extended);


#if defined(BOOST_GEOMETRY_USE_RESCALING)
    std::cout << "RESCALED - Expected: " << for_rescaling << " error(s)" << std::endl;
#elif defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE) && defined(BOOST_GEOMETRY_TEST_ONLY_ONE_ORDER)
    std::cout << "NOT RESCALED - Expected: " << for_no_rescaling << " error(s)" << std::endl;
#else
    std::cout << std::endl;
#endif
}

inline void BoostGeometryWriteExpectedFailures(std::size_t for_rescaling,
                                               std::size_t for_no_rescaling_double = BG_NO_FAILURES)
{
    BoostGeometryWriteExpectedFailures(for_rescaling, for_no_rescaling_double,
                                       for_no_rescaling_double, for_no_rescaling_double);
}

#endif

#endif // GEOMETRY_TEST_GEOMETRY_TEST_COMMON_HPP
