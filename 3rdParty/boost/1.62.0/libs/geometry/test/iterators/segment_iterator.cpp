// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_segment_iterator
#endif

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <boost/test/included/unit_test.hpp>

#include <boost/assign/list_of.hpp>
#include <boost/concept_check.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/iterator/iterator_concepts.hpp>
#include <boost/tuple/tuple.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/geometries/register/linestring.hpp>
#include <boost/geometry/geometries/register/multi_linestring.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/io/dsv/write.hpp>

#include <boost/geometry/core/closure.hpp>

#include <boost/geometry/algorithms/convert.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/algorithms/num_segments.hpp>

#include <boost/geometry/policies/compare.hpp>

#include <boost/geometry/iterators/segment_iterator.hpp>

#include <test_common/with_pointer.hpp>
#include <test_geometries/copy_on_dereference_geometries.hpp>

namespace ba = ::boost::assign;
namespace bg = ::boost::geometry;
namespace bgm = bg::model;

typedef bgm::point<double, 2, bg::cs::cartesian> point_type;
typedef bgm::linestring<point_type> linestring_type;
typedef bgm::ring<point_type, true, true> ring_cw_closed_type;
typedef bgm::ring<point_type, true, false> ring_cw_open_type;
typedef bgm::polygon<point_type, true, true> polygon_cw_closed_type;
typedef bgm::polygon<point_type, true, false> polygon_cw_open_type;

// multi-geometries
typedef bgm::multi_linestring<linestring_type> multi_linestring_type;
typedef bgm::multi_polygon<polygon_cw_closed_type> multi_polygon_cw_closed_type;
typedef bgm::multi_polygon<polygon_cw_open_type> multi_polygon_cw_open_type;

// tuple-based geometries
typedef boost::tuple<double, double> tuple_point_type;
typedef std::vector<tuple_point_type> tuple_linestring_type;
typedef std::vector<tuple_linestring_type> tuple_multi_linestring_type;

BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_LINESTRING(tuple_linestring_type)
BOOST_GEOMETRY_REGISTER_MULTI_LINESTRING(tuple_multi_linestring_type)

BOOST_GEOMETRY_REGISTER_LINESTRING_TEMPLATED(std::vector)


template <typename Geometry>
inline Geometry from_wkt(std::string const& wkt)
{
    Geometry geometry;
    boost::geometry::read_wkt(wkt, geometry);
    return geometry;
}

template <typename Iterator>
inline std::ostream& print_geometry_range(std::ostream& os,
                                          Iterator first,
                                          Iterator beyond,
                                          std::string const& header)
{
    os << header << "(";
    for (Iterator it = first; it != beyond; ++it)
    {
        os << " " << bg::dsv(*it);
    }
    os << " )";
    return os;
}

template <typename Geometry>
struct test_iterator_concepts
{
    typedef bg::segment_iterator<Geometry> iterator;
    BOOST_CONCEPT_ASSERT(( boost::BidirectionalIteratorConcept<iterator> ));
    BOOST_CONCEPT_ASSERT(( boost_concepts::ReadableIteratorConcept<iterator> ));
    BOOST_CONCEPT_ASSERT
        (( boost_concepts::BidirectionalTraversalConcept<iterator> ));
};

struct equals
{
    template <typename Iterator>
    static inline std::size_t number_of_elements(Iterator begin,
                                                 Iterator end)
    {
        std::size_t size = std::distance(begin, end);

        std::size_t num_elems(0);
        for (Iterator it = begin; it != end; ++it)
        {
            ++num_elems;
        }
        BOOST_CHECK(size == num_elems);

        num_elems = 0;
        for (Iterator it = end; it != begin; --it)
        {
            ++num_elems;
        }
        BOOST_CHECK(size == num_elems);

        return num_elems;
    }

    template <typename Iterator1, typename Iterator2>
    static inline bool apply(Iterator1 begin1, Iterator1 end1,
                             Iterator2 begin2, Iterator2 end2)
    {
        std::size_t num_points1 = number_of_elements(begin1, end1);
        std::size_t num_points2 = number_of_elements(begin2, end2);

        if (num_points1 != num_points2)
        {
            return false;
        }

        Iterator1 it1 = begin1;
        Iterator2 it2 = begin2;
        for (; it1 != end1; ++it1, ++it2)
        {
            if (! bg::equals(*it1, *it2))
            {
                return false;
            }
        }
        return true;
    }
};


template <typename Geometry, typename SegmentRange>
struct test_segment_iterator_of_geometry
{
    template <typename G>
    static inline void base_test(G const& geometry,
                                 SegmentRange const& segment_range,
                                 std::string const& header,
                                 bool check_num_segments)
    {
        typedef bg::segment_iterator<G const> segment_iterator;

        test_iterator_concepts<G const>();

        segment_iterator begin = bg::segments_begin(geometry);
        segment_iterator end = bg::segments_end(geometry);

        if (check_num_segments)
        {
            BOOST_CHECK(std::size_t(std::distance(begin, end))
                        ==
                        bg::num_segments(geometry));
        }

        BOOST_CHECK(equals::apply(begin, end,
                                  bg::segments_begin(segment_range),
                                  bg::segments_end(segment_range))
                    );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::string closure
            = (
               (bg::closure<Geometry>::value == bg::closed)
               ? "closed"
               : "open"
               );

        std::cout << header << " geometry (WKT): "
                  << bg::wkt(geometry) << std::endl;
        std::cout << header << " geometry (DSV): "
                  << bg::dsv(geometry) << std::endl;
        std::cout << "geometry's closure: " << closure << std::endl;
        print_geometry_range(std::cout, begin, end, "segment range: ");

        std::cout << std::endl;

        print_geometry_range(std::cout,
                             bg::segments_begin(segment_range),
                             bg::segments_end(segment_range),
                             "expected segment range: ");
        std::cout << std::endl;
#endif

        // testing dereferencing
        typedef typename std::iterator_traits
            <
                segment_iterator
            >::value_type value_type;

        if (bg::segments_begin(geometry) != bg::segments_end(geometry))
        {
            value_type first_segment = *bg::segments_begin(geometry);

            boost::ignore_unused(first_segment);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            typedef bg::model::segment
                <
                    bg::model::point<double, 2, bg::cs::cartesian>
                > other_segment;
            other_segment other_seg;
            // convert is used as a workaround for geometries whose
            // point is a pointer. WKT does not seem to work for
            // segment iterators created this way.
            bg::convert(first_segment, other_seg);
            std::cout << "first segment in geometry: "
                      << bg::wkt(other_seg)
                      << std::endl;
            std::cout << "first segment in geometry (DSV): "
                      << bg::dsv(first_segment)
                      << std::endl;

            std::cout << std::endl << std::endl;
#endif
        }

        // test copying all segments to a vector
        std::vector<value_type> segments;
        std::copy(bg::segments_begin(geometry),
                  bg::segments_end(geometry),
                  std::back_inserter(segments));

        BOOST_CHECK(std::size_t( std::distance(bg::segments_begin(geometry),
                                               bg::segments_end(geometry)) )
                    ==
                    segments.size());
    }

    static inline void apply(Geometry geometry,
                             SegmentRange const& segment_range,
                             bool check_num_segments = true)
    {
        base_test<Geometry>(geometry, segment_range, "const",
                            check_num_segments);
    }
};

//======================================================================
//======================================================================

template <typename ClosedGeometry, typename ExpectedResult>
struct dual_tester
{
    template <typename OpenGeometry>
    static inline void apply(OpenGeometry const& open_g,
                             ExpectedResult expected,
                             bool check_num_segments = true)
    {
        typedef test_segment_iterator_of_geometry
            <
                OpenGeometry, ExpectedResult
            > otester;

        typedef test_segment_iterator_of_geometry
            <
                ClosedGeometry, ExpectedResult
            > ctester;

        otester::apply(open_g, expected, check_num_segments);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl << std::endl;
#endif

        ClosedGeometry closed_g;

        bg::convert(open_g, closed_g);

        ctester::apply(closed_g, expected, check_num_segments);
    }
};

//======================================================================
//======================================================================

BOOST_AUTO_TEST_CASE( test_linestring_segment_iterator )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "*** LINESTRING ***" << std::endl;
#endif

    typedef tuple_multi_linestring_type TML;
    typedef linestring_type G;

    typedef test_segment_iterator_of_geometry<G, TML> tester;

    tester::apply(from_wkt<G>("LINESTRING(0 0,1 1,2 2,3 3,4 4)"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(1,1) )
                  ( ba::tuple_list_of(1,1)(2,2) )
                  ( ba::tuple_list_of(2,2)(3,3) )
                  ( ba::tuple_list_of(3,3)(4,4) )
                  );

    // linestring with no points
    tester::apply(from_wkt<G>("LINESTRING()"),
                  ba::list_of<tuple_linestring_type>()
                  );

    // linestring with a single point
    tester::apply(from_wkt<G>("LINESTRING(1 0)"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(1,0)(1,0) ),
                  false
                  );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
#endif
}

//======================================================================
//======================================================================

BOOST_AUTO_TEST_CASE( test_ring_segment_iterator )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "*** RING ***" << std::endl;
#endif

    typedef tuple_multi_linestring_type TML;
    typedef ring_cw_open_type OG;
    typedef ring_cw_closed_type CG;

    typedef dual_tester<CG, TML> tester;

    tester::apply(from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(10,0) )
                  ( ba::tuple_list_of(10,0)(0,0) )
                  );

    // open ring with no points
    tester::apply(from_wkt<OG>("POLYGON(())"),
                  ba::list_of<tuple_linestring_type>()
                  );

    // open ring with a single point (one segment)
    tester::apply(from_wkt<OG>("POLYGON((0 0))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,0) ),
                  false
                  );

    // open ring with a two points (two segments)
    tester::apply(from_wkt<OG>("POLYGON((0 0,0 10))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(0,0) )
                  );

    // open ring with a three points (three segments)
    tester::apply(from_wkt<OG>("POLYGON((0 0,0 10,10 10))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(0,0) )
                  );

    tester::apply(from_wkt<CG>("POLYGON((0 0,0 10,10 10,10 0,0 0))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(10,0) )
                  ( ba::tuple_list_of(10,0)(0,0) )
                  );

    // closed ring with no points
    tester::apply(from_wkt<CG>("POLYGON(())"),
                  ba::list_of<tuple_linestring_type>()
                  );

    // closed ring with a single point (one segment)
    tester::apply(from_wkt<CG>("POLYGON((0 0))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,0) ),
                  false
                  );

    // closed ring with two points (one segment)
    tester::apply(from_wkt<CG>("POLYGON((0 0,0 0))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,0) )
                  );

    // closed ring with three points (two segments)
    tester::apply(from_wkt<CG>("POLYGON((0 0,0 10,0 0))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(0,0) )
                  );

    // closed ring with four points (three segments)
    tester::apply(from_wkt<CG>("POLYGON((0 0,0 10,10 10,0 0))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(0,0) )
                  );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
#endif
}

//======================================================================
//======================================================================

BOOST_AUTO_TEST_CASE( test_polygon_segment_iterator )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "*** POLYGON ***" << std::endl;
#endif

    typedef tuple_multi_linestring_type TML;
    typedef polygon_cw_open_type OG;
    typedef polygon_cw_closed_type CG;

    typedef dual_tester<CG, TML> tester;

    tester::apply(from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(1 1,9 1,9 9,1 9))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(10,0) )
                  ( ba::tuple_list_of(10,0)(0,0) )
                  ( ba::tuple_list_of(1,1)(9,1) )
                  ( ba::tuple_list_of(9,1)(9,9) )
                  ( ba::tuple_list_of(9,9)(1,9) )
                  ( ba::tuple_list_of(1,9)(1,1) )
                  );

    // open polygon with no points
    tester::apply(from_wkt<OG>("POLYGON(())"),
                  ba::list_of<tuple_linestring_type>()
                  );

    // open polygons with single-point rings
    tester::apply(from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(1 1))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(10,0) )
                  ( ba::tuple_list_of(10,0)(0,0) )
                  ( ba::tuple_list_of(1,1)(1,1) ),
                  false
                  );

    tester::apply(from_wkt<OG>("POLYGON((0 0),(1 1,9 1,9 9,1 9))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,0) )
                  ( ba::tuple_list_of(1,1)(9,1) )
                  ( ba::tuple_list_of(9,1)(9,9) )
                  ( ba::tuple_list_of(9,9)(1,9) )
                  ( ba::tuple_list_of(1,9)(1,1) ),
                  false
                  );


    tester::apply(from_wkt<CG>("POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,9 1,9 9,1 9,1 1))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(10,0) )
                  ( ba::tuple_list_of(10,0)(0,0) )
                  ( ba::tuple_list_of(1,1)(9,1) )
                  ( ba::tuple_list_of(9,1)(9,9) )
                  ( ba::tuple_list_of(9,9)(1,9) )
                  ( ba::tuple_list_of(1,9)(1,1) )
                  );

    // closed polygons with no points
    tester::apply(from_wkt<CG>("POLYGON(())"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<CG>("POLYGON((),())"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<CG>("POLYGON((),(),())"),
                  ba::list_of<tuple_linestring_type>()
                  );

    // closed polygons with single-point rings
    tester::apply(from_wkt<CG>("POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(10,0) )
                  ( ba::tuple_list_of(10,0)(0,0) )
                  ( ba::tuple_list_of(1,1)(1,1) ),
                  false
                  );

    tester::apply(from_wkt<CG>("POLYGON((0 0),(1 1,9 1,9 9,1 9,1 1))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,0) )
                  ( ba::tuple_list_of(1,1)(9,1) )
                  ( ba::tuple_list_of(9,1)(9,9) )
                  ( ba::tuple_list_of(9,9)(1,9) )
                  ( ba::tuple_list_of(1,9)(1,1) ),
                  false
                  );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
#endif
}

//======================================================================
//======================================================================

BOOST_AUTO_TEST_CASE( test_multi_linestring_segment_iterator )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "*** MULTILINESTRING ***" << std::endl;
#endif

    typedef tuple_multi_linestring_type TML;
    typedef multi_linestring_type G;

    typedef test_segment_iterator_of_geometry<G, TML> tester;

    tester::apply(from_wkt<G>("MULTILINESTRING((0 0,1 1,2 2,3 3,4 4),(5 5,6 6,7 7,8 8),(9 9,10 10))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(1,1) )
                  ( ba::tuple_list_of(1,1)(2,2) )
                  ( ba::tuple_list_of(2,2)(3,3) )
                  ( ba::tuple_list_of(3,3)(4,4) )
                  ( ba::tuple_list_of(5,5)(6,6) )
                  ( ba::tuple_list_of(6,6)(7,7) )
                  ( ba::tuple_list_of(7,7)(8,8) )
                  ( ba::tuple_list_of(9,9)(10,10) )
                  );

    // empty multi-linestrings
    tester::apply(from_wkt<G>("MULTILINESTRING()"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<G>("MULTILINESTRING(())"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<G>("MULTILINESTRING((),())"),
                  ba::list_of<tuple_linestring_type>()
                  );

    // multi-linestring with a linestring with one point
    tester::apply(from_wkt<G>("MULTILINESTRING((0 0,1 1,2 2,3 3,4 4),(5 5),(9 9,10 10))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(1,1) )
                  ( ba::tuple_list_of(1,1)(2,2) )
                  ( ba::tuple_list_of(2,2)(3,3) )
                  ( ba::tuple_list_of(3,3)(4,4) )
                  ( ba::tuple_list_of(5,5)(5,5) )
                  ( ba::tuple_list_of(9,9)(10,10) ),
                  false
                  );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
#endif
}

//======================================================================
//======================================================================

BOOST_AUTO_TEST_CASE( test_multi_polygon_segment_iterator )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "*** MULTIPOLYGON ***" << std::endl;
#endif

    typedef tuple_multi_linestring_type TML;
    typedef multi_polygon_cw_open_type OG;
    typedef multi_polygon_cw_closed_type CG;

    typedef dual_tester<CG, TML> tester;

    tester::apply(from_wkt<OG>("MULTIPOLYGON(((0 0,0 10,10 10,10 0),(1 1,9 1,9 9,1 9)),((20 0,20 10,30 10,30 0),(21 1,29 1,29 9,21 9)))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(10,0) )
                  ( ba::tuple_list_of(10,0)(0,0) )
                  ( ba::tuple_list_of(1,1)(9,1) )
                  ( ba::tuple_list_of(9,1)(9,9) )
                  ( ba::tuple_list_of(9,9)(1,9) )
                  ( ba::tuple_list_of(1,9)(1,1) )
                  ( ba::tuple_list_of(20,0)(20,10) )
                  ( ba::tuple_list_of(20,10)(30,10) )
                  ( ba::tuple_list_of(30,10)(30,0) )
                  ( ba::tuple_list_of(30,0)(20,0) )
                  ( ba::tuple_list_of(21,1)(29,1) )
                  ( ba::tuple_list_of(29,1)(29,9) )
                  ( ba::tuple_list_of(29,9)(21,9) )
                  ( ba::tuple_list_of(21,9)(21,1) )
                  );

    tester::apply(from_wkt<CG>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(1 1,9 1,9 9,1 9,1 1)),((20 0,20 10,30 10,30 0,20 0),(21 1,29 1,29 9,21 9,21 1)))"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(0,0)(0,10) )
                  ( ba::tuple_list_of(0,10)(10,10) )
                  ( ba::tuple_list_of(10,10)(10,0) )
                  ( ba::tuple_list_of(10,0)(0,0) )
                  ( ba::tuple_list_of(1,1)(9,1) )
                  ( ba::tuple_list_of(9,1)(9,9) )
                  ( ba::tuple_list_of(9,9)(1,9) )
                  ( ba::tuple_list_of(1,9)(1,1) )
                  ( ba::tuple_list_of(20,0)(20,10) )
                  ( ba::tuple_list_of(20,10)(30,10) )
                  ( ba::tuple_list_of(30,10)(30,0) )
                  ( ba::tuple_list_of(30,0)(20,0) )
                  ( ba::tuple_list_of(21,1)(29,1) )
                  ( ba::tuple_list_of(29,1)(29,9) )
                  ( ba::tuple_list_of(29,9)(21,9) )
                  ( ba::tuple_list_of(21,9)(21,1) )
                  );

    // test empty closed multi-polygons
    tester::apply(from_wkt<CG>("MULTIPOLYGON()"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<CG>("MULTIPOLYGON((()))"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<CG>("MULTIPOLYGON(((),()))"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<CG>("MULTIPOLYGON(((),(),()))"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<CG>("MULTIPOLYGON(((),(),()),(()))"),
                  ba::list_of<tuple_linestring_type>()
                  );
    tester::apply(from_wkt<CG>("MULTIPOLYGON(((),(),()),((),()))"),
                  ba::list_of<tuple_linestring_type>()
                  );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
#endif
}

//======================================================================
//======================================================================

BOOST_AUTO_TEST_CASE( test_linestring_of_point_pointers )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "*** LINESTRING OF POINT POINTERS ***" << std::endl;
#endif

    typedef tuple_multi_linestring_type TML;
    typedef std::vector<test::test_point_xy*> L;

    std::vector<test::test_point_xy*> linestring;
    for (int i = 1; i < 10; i++)
    {
        test::test_point_xy* p = new test::test_point_xy;
        p->x = i;
        p->y = -i;
        linestring.push_back(p);
    }

    test::test_point_xy* zero = new test::test_point_xy;
    zero->x = 0;
    zero->y = 0;

    typedef test_segment_iterator_of_geometry<L, TML> tester;

    tester::apply(linestring,
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(1,-1)(2,-2) )
                  ( ba::tuple_list_of(2,-2)(3,-3) )
                  ( ba::tuple_list_of(3,-3)(4,-4) )
                  ( ba::tuple_list_of(4,-4)(5,-5) )
                  ( ba::tuple_list_of(5,-5)(6,-6) )
                  ( ba::tuple_list_of(6,-6)(7,-7) )
                  ( ba::tuple_list_of(7,-7)(8,-8) )
                  ( ba::tuple_list_of(8,-8)(9,-9) )
                  );

    for (unsigned int i = 0; i < linestring.size(); i++)
    {
        delete linestring[i];
    }
}

//======================================================================
//======================================================================

BOOST_AUTO_TEST_CASE( test_linestring_copy_on_dereference )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "*** LINESTRING WITH COPY-ON-DEREFERENCE ITERATOR ***"
              << std::endl;
#endif

    typedef tuple_multi_linestring_type TML;
    typedef linestring_copy_on_dereference<point_type> L;

    typedef test_segment_iterator_of_geometry<L, TML> tester;

    tester::apply(from_wkt<L>("LINESTRING(1 -1,2 -2,3 -3,4 -4,5 -5,6 -6, 7 -7,8 -8,9 -9)"),
                  ba::list_of<tuple_linestring_type>
                  ( ba::tuple_list_of(1,-1)(2,-2) )
                  ( ba::tuple_list_of(2,-2)(3,-3) )
                  ( ba::tuple_list_of(3,-3)(4,-4) )
                  ( ba::tuple_list_of(4,-4)(5,-5) )
                  ( ba::tuple_list_of(5,-5)(6,-6) )
                  ( ba::tuple_list_of(6,-6)(7,-7) )
                  ( ba::tuple_list_of(7,-7)(8,-8) )
                  ( ba::tuple_list_of(8,-8)(9,-9) )
                  );
}
