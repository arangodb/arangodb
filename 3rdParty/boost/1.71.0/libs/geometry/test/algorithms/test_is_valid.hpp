// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_IS_VALID_HPP
#define BOOST_GEOMETRY_TEST_IS_VALID_HPP

#include <iostream>
#include <sstream>
#include <string>

#include <boost/core/ignore_unused.hpp>
#include <boost/range.hpp>
#include <boost/variant/variant.hpp>

#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/exterior_ring.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/algorithms/convert.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>

#include <boost/geometry/algorithms/detail/check_iterator_range.hpp>

#include <from_wkt.hpp>

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#include "pretty_print_geometry.hpp"
#endif


namespace bg = ::boost::geometry;

typedef bg::model::point<double, 2, bg::cs::cartesian> point_type;
typedef bg::model::segment<point_type>                 segment_type;
typedef bg::model::box<point_type>                     box_type;
typedef bg::model::linestring<point_type>              linestring_type;
typedef bg::model::multi_linestring<linestring_type>   multi_linestring_type;
typedef bg::model::multi_point<point_type>             multi_point_type;


//----------------------------------------------------------------------------


// returns true if a geometry can be converted to closed
template
<
    typename Geometry,
    typename Tag = typename bg::tag<Geometry>::type,
    bg::closure_selector Closure = bg::closure<Geometry>::value
>
struct is_convertible_to_closed
{
    static inline bool apply(Geometry const&)
    {
        return false;
    }
};

template <typename Ring>
struct is_convertible_to_closed<Ring, bg::ring_tag, bg::open>
{
    static inline bool apply(Ring const& ring)
    {
        return boost::size(ring) > 0;
    }
};

template <typename Polygon>
struct is_convertible_to_closed<Polygon, bg::polygon_tag, bg::open>
{
    typedef typename bg::ring_type<Polygon>::type ring_type;

    template <typename InteriorRings>
    static inline
    bool apply_to_interior_rings(InteriorRings const& interior_rings)
    {
        return bg::detail::check_iterator_range
            <
                is_convertible_to_closed<ring_type>
            >::apply(boost::begin(interior_rings),
                     boost::end(interior_rings));
    }

    static inline bool apply(Polygon const& polygon)
    {
        return boost::size(bg::exterior_ring(polygon)) > 0
            && apply_to_interior_rings(bg::interior_rings(polygon));
    }
};

template <typename MultiPolygon>
struct is_convertible_to_closed<MultiPolygon, bg::multi_polygon_tag, bg::open>
{
    typedef typename boost::range_value<MultiPolygon>::type polygon;

    static inline bool apply(MultiPolygon const& multi_polygon)
    {
        return bg::detail::check_iterator_range
            <
                is_convertible_to_closed<polygon>,
                false // do not allow empty multi-polygon
            >::apply(boost::begin(multi_polygon),
                     boost::end(multi_polygon));
    }
};


//----------------------------------------------------------------------------


// returns true if a geometry can be converted to cw
template
<
    typename Geometry,
    typename Tag = typename bg::tag<Geometry>::type,
    bg::order_selector Order = bg::point_order<Geometry>::value
>
struct is_convertible_to_cw
{
    static inline bool apply(Geometry const&)
    {
        return bg::point_order<Geometry>::value == bg::counterclockwise;
    }
};


//----------------------------------------------------------------------------


// returns true if geometry can be converted to polygon
template
<
    typename Geometry,
    typename Tag = typename bg::tag<Geometry>::type
>
struct is_convertible_to_polygon
{
    typedef Geometry type;
    static bool const value = false;
};

template <typename Ring>
struct is_convertible_to_polygon<Ring, bg::ring_tag>
{
    typedef bg::model::polygon
        <
            typename bg::point_type<Ring>::type,
            bg::point_order<Ring>::value == bg::clockwise,
            bg::closure<Ring>::value == bg::closed
        > type;

    static bool const value = true;
};


//----------------------------------------------------------------------------


// returns true if geometry can be converted to multi-polygon
template
<
    typename Geometry,
    typename Tag = typename bg::tag<Geometry>::type
>
struct is_convertible_to_multipolygon
{
    typedef Geometry type;
    static bool const value = false;
};

template <typename Ring>
struct is_convertible_to_multipolygon<Ring, bg::ring_tag>
{
    typedef bg::model::multi_polygon
        <
            typename is_convertible_to_polygon<Ring>::type
        > type;

    static bool const value = true;
};

template <typename Polygon>
struct is_convertible_to_multipolygon<Polygon, bg::polygon_tag>
{
    typedef bg::model::multi_polygon<Polygon> type;
    static bool const value = true;
};


//----------------------------------------------------------------------------


template <typename ValidityTester>
struct validity_checker
{
    template <typename Geometry>
    static inline bool apply(std::string const& case_id,
                             Geometry const& geometry,
                             bool expected_result,
                             std::string& reason)
    {
        bool valid = ValidityTester::apply(geometry);
        std::string const reason_valid
            = bg::validity_failure_type_message(bg::no_failure);
        reason = ValidityTester::reason(geometry);
        std::string reason_short = reason.substr(0, reason_valid.length());

        BOOST_CHECK_MESSAGE(valid == expected_result,
            "case id: " << case_id
            << ", Expected: " << expected_result
            << ", detected: " << valid
            << "; wkt: " << bg::wkt(geometry));

        BOOST_CHECK_MESSAGE(reason != "",
            "case id (empty reason): " << case_id
            << ", Expected: " << valid
            << ", detected reason: " << reason
            << "; wkt: " << bg::wkt(geometry));

        BOOST_CHECK_MESSAGE((valid && reason == reason_valid)
                            ||
                            (! valid && reason != reason_valid)
                            ||
                            (! valid && reason_short != reason_valid),
            "case id (reason): " << case_id
            << ", Expected: " << valid
            << ", detected reason: " << reason
            << "; wkt: " << bg::wkt(geometry));

        return valid;
    }
};


//----------------------------------------------------------------------------


struct default_validity_tester
{
    template <typename Geometry>    
    static inline bool apply(Geometry const& geometry)
    {
        return bg::is_valid(geometry);
    }

    template <typename Geometry>
    static inline std::string reason(Geometry const& geometry)
    {
        std::string message;
        bg::is_valid(geometry, message);
        return message;
    }
};


template <bool AllowSpikes>
struct validity_tester_linear
{
    template <typename Geometry>
    static inline bool apply(Geometry const& geometry)
    {
        bool const irrelevant = true;
        bg::is_valid_default_policy<irrelevant, AllowSpikes> visitor;
        return bg::is_valid(geometry, visitor, bg::default_strategy());
    }

    template <typename Geometry>
    static inline std::string reason(Geometry const& geometry)
    {
        bool const irrelevant = true;
        std::ostringstream oss;
        bg::failing_reason_policy<irrelevant, AllowSpikes> visitor(oss);
        bg::is_valid(geometry, visitor, bg::default_strategy());
        return oss.str();
    }
};


template <bool AllowDuplicates>
struct validity_tester_areal
{
    template <typename Geometry>
    static inline bool apply(Geometry const& geometry)
    {
        bg::is_valid_default_policy<AllowDuplicates> visitor;
        return bg::is_valid(geometry, visitor, bg::default_strategy());
    }

    template <typename Geometry>
    static inline std::string reason(Geometry const& geometry)
    {
        std::ostringstream oss;
        bg::failing_reason_policy<AllowDuplicates> visitor(oss);
        bg::is_valid(geometry, visitor, bg::default_strategy());
        return oss.str();
    }

};


template <bool AllowDuplicates>
struct validity_tester_geo_areal
{
    template <typename Geometry>
    static inline bool apply(Geometry const& geometry)
    {
        bg::is_valid_default_policy<AllowDuplicates> visitor;
        bg::strategy::intersection::geographic_segments<> s;
        return bg::is_valid(geometry, visitor, s);
    }

    template <typename Geometry>
    static inline std::string reason(Geometry const& geometry)
    {
        std::ostringstream oss;
        bg::failing_reason_policy<AllowDuplicates> visitor(oss);
        bg::strategy::intersection::geographic_segments<> s;
        bg::is_valid(geometry, visitor, s);
        return oss.str();
    }

};


//----------------------------------------------------------------------------


template
<
    typename ValidityTester,
    typename Geometry,
    typename ClosedGeometry = Geometry,
    typename CWGeometry = Geometry,
    typename CWClosedGeometry = Geometry,
    typename Tag = typename bg::tag<Geometry>::type
>
class test_valid
{
protected:
    template <typename G>
    static inline void base_test(std::string const& case_id,
                                 G const& g,
                                 bool expected_result)
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "=======" << std::endl;
#endif

        std::string reason;
        bool valid = validity_checker
            <
                ValidityTester
            >::apply(case_id, g, expected_result, reason);
        boost::ignore_unused(valid);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "case id: " << case_id << ", Geometry: ";
        pretty_print_geometry<G>::apply(std::cout, g);
        std::cout << std::endl;
        std::cout << "wkt: " << bg::wkt(g) << std::endl;
        std::cout << std::boolalpha;
        std::cout << "is valid? " << valid << std::endl;
        std::cout << "expected result: " << expected_result << std::endl;
        std::cout << "reason: " << reason << std::endl;
        std::cout << "=======" << std::endl;
        std::cout << std::noboolalpha;
#endif
    }

public:
    static inline void apply(std::string const& case_id,
                             Geometry const& geometry,
                             bool expected_result)
    {
        std::stringstream sstr;
        sstr << case_id << "-original"; // which is: CCW open
        base_test(sstr.str(), geometry, expected_result);

        if ( is_convertible_to_closed<Geometry>::apply(geometry) )
        {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "...checking closed geometry..."
                      << std::endl;
#endif
            ClosedGeometry closed_geometry;
            bg::convert(geometry, closed_geometry);
            sstr.str("");
            sstr << case_id << "-2closed";
            base_test(sstr.str(), closed_geometry, expected_result);
        }
        if ( is_convertible_to_cw<Geometry>::apply(geometry) )
        {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "...checking cw open geometry..."
                      << std::endl;
#endif            
            CWGeometry cw_geometry;
            bg::convert(geometry, cw_geometry);
            sstr.str("");
            sstr << case_id << "-2CW";
            base_test(sstr.str(), cw_geometry, expected_result);
            if ( is_convertible_to_closed<CWGeometry>::apply(cw_geometry) )
            {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
                std::cout << "...checking cw closed geometry..."
                          << std::endl;
#endif            
                CWClosedGeometry cw_closed_geometry;
                bg::convert(cw_geometry, cw_closed_geometry);
                sstr.str("");
                sstr << case_id << "-2CWclosed";
                base_test(sstr.str(), cw_closed_geometry, expected_result);
            }
        }

        if ( BOOST_GEOMETRY_CONDITION(is_convertible_to_polygon<Geometry>::value) )
        {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "...checking geometry converted to polygon..."
                      << std::endl;
#endif            
            typename is_convertible_to_polygon<Geometry>::type polygon;
            bg::convert(geometry, polygon);
            sstr.str("");
            sstr << case_id << "-2Polygon";
            base_test(sstr.str(), polygon, expected_result);
        }

        if ( BOOST_GEOMETRY_CONDITION(is_convertible_to_multipolygon<Geometry>::value) )
        {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "...checking geometry converted to multi-polygon..."
                      << std::endl;
#endif            
            typename is_convertible_to_multipolygon
                <
                    Geometry
                >::type multipolygon;

            bg::convert(geometry, multipolygon);
            sstr.str("");
            sstr << case_id << "-2MultiPolygon";
            base_test(sstr.str(), multipolygon, expected_result);
        }

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl << std::endl << std::endl;
#endif
    }

    static inline void apply(std::string const& case_id,
                             std::string const& wkt,
                             bool expected_result)
    {
        apply(case_id, from_wkt<Geometry>(wkt), expected_result);
    }
};


//----------------------------------------------------------------------------


template <typename VariantGeometry>
class test_valid_variant
    : test_valid<default_validity_tester, VariantGeometry>
{
private:
    typedef test_valid<default_validity_tester, VariantGeometry> base_type;

public:
    static inline void apply(std::string const& case_id,
                             VariantGeometry const& vg,
                             bool expected_result)
    {
        std::ostringstream oss;
        base_type::base_test(case_id, vg, expected_result);
    }
};


#endif // BOOST_GEOMETRY_TEST_IS_VALID_HPP
