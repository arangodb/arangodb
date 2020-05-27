// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2017, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#ifndef BOOST_GEOMETRY_TEST_INTERSECTION_LINEAR_LINEAR_HPP
#define BOOST_GEOMETRY_TEST_INTERSECTION_LINEAR_LINEAR_HPP

#include <limits>

#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/geometry.hpp>
#include "../test_set_ops_linear_linear.hpp"
#include <from_wkt.hpp>
#include <to_svg.hpp>


//==================================================================
//==================================================================
// intersection of (linear) geometries
//==================================================================
//==================================================================

template <typename Geometry1, typename Geometry2, typename MultiLineString>
inline void check_result(Geometry1 const& geometry1,
                         Geometry2 const& geometry2,
                         MultiLineString const& mls_output,
                         MultiLineString const& mls_int1,
                         MultiLineString const& mls_int2,
                         std::string const& case_id,
                         double tolerance)
{
    BOOST_CHECK_MESSAGE( equals::apply(mls_int1, mls_output, tolerance)
                         || equals::apply(mls_int2, mls_output, tolerance),
                         "case id: " << case_id
                         << ", intersection L/L: " << bg::wkt(geometry1)
                         << " " << bg::wkt(geometry2)
                         << " -> Expected: " << bg::wkt(mls_int1)
                         << " or: " << bg::wkt(mls_int2)
                         << " computed: " << bg::wkt(mls_output) );
}

template
<
    typename Geometry1, typename Geometry2,
    typename MultiLineString
>
class test_intersection_of_geometries
{
private:
    static inline void base_test(Geometry1 const& geometry1,
                                 Geometry2 const& geometry2,
                                 MultiLineString const& mls_int1,
                                 MultiLineString const& mls_int2,
                                 std::string const& case_id,
                                 double tolerance,
                                 bool test_vector_and_deque = false)
    {
        static bool vector_deque_already_tested = false;

        typedef typename boost::range_value<MultiLineString>::type LineString;
        typedef std::vector<LineString> linestring_vector;
        typedef std::deque<LineString> linestring_deque;

        MultiLineString mls_output;

        linestring_vector ls_vector_output;
        linestring_deque ls_deque_output;

        // Check normal behaviour
        bg::intersection(geometry1, geometry2, mls_output);

        check_result(geometry1, geometry2, mls_output, mls_int1, mls_int2, case_id, tolerance);

        // Check strategy passed explicitly
        typedef typename bg::strategy::relate::services::default_strategy
            <
                Geometry1, Geometry2
            >::type strategy_type;
        bg::clear(mls_output);
        bg::intersection(geometry1, geometry2, mls_output, strategy_type());

        check_result(geometry1, geometry2, mls_output, mls_int1, mls_int2, case_id, tolerance);

        set_operation_output("intersection", case_id,
                             geometry1, geometry2, mls_output);
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "Geometry #1: " << bg::wkt(geometry1) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(geometry2) << std::endl;
        std::cout << "intersection : " << bg::wkt(mls_output) << std::endl;
        std::cout << "expected intersection : " << bg::wkt(mls_int1)
                  << " or: " << bg::wkt(mls_int2) << std::endl;
        std::cout << std::endl;
        std::cout << "************************************" << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
#endif

        if ( !vector_deque_already_tested && test_vector_and_deque )
        {
            vector_deque_already_tested = true;
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << std::endl;
            std::cout << "Testing with vector and deque as output container..."
                      << std::endl;
#endif
            bg::intersection(geometry1, geometry2, ls_vector_output);
            bg::intersection(geometry1, geometry2, ls_deque_output);

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(mls_int1, ls_vector_output, tolerance));

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(mls_int1, ls_deque_output, tolerance));

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "Done!" << std::endl << std::endl;
#endif
        }

        // check the intersection where the order of the two
        // geometries is reversed
        bg::clear(mls_output);
        bg::intersection(geometry2, geometry1, mls_output);

        check_result(geometry1, geometry2, mls_output, mls_int1, mls_int2, case_id, tolerance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "Geometry #1: " << bg::wkt(geometry2) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(geometry1) << std::endl;
        std::cout << "intersection : " << bg::wkt(mls_output) << std::endl;
        std::cout << "expected intersection : " << bg::wkt(mls_int1)
                  << " or: " << bg::wkt(mls_int2) << std::endl;
        std::cout << std::endl;
        std::cout << "************************************" << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    static inline void base_test_all(Geometry1 const& geometry1,
                                     Geometry2 const& geometry2)
    {
        typedef typename bg::point_type<MultiLineString>::type Point;
        typedef bg::model::multi_point<Point> multi_point;

        MultiLineString mls12_output, mls21_output;
        multi_point mp12_output, mp21_output;

        bg::intersection(geometry1, geometry2, mls12_output);
        bg::intersection(geometry1, geometry2, mp12_output);
        bg::intersection(geometry2, geometry1, mls21_output);
        bg::intersection(geometry2, geometry1, mp21_output);

        std::cout << "************************************" << std::endl;
        std::cout << "Geometry #1: " << bg::wkt(geometry1) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(geometry2) << std::endl;
        std::cout << "intersection(1,2) [MLS]: " << bg::wkt(mls12_output)
                  << std::endl;
        std::cout << "intersection(2,1) [MLS]: " << bg::wkt(mls21_output)
                  << std::endl;
        std::cout << std::endl;
        std::cout << "intersection(1,2) [MP]: " << bg::wkt(mp12_output)
                  << std::endl;
        std::cout << "intersection(2,1) [MP]: " << bg::wkt(mp21_output)
                  << std::endl;
        std::cout << std::endl;
        std::cout << "************************************" << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
    }
#else
    static inline void base_test_all(Geometry1 const&, Geometry2 const&)
    {
    }
#endif


public:
    static inline void apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             MultiLineString const& mls_int1,
                             MultiLineString const& mls_int2,
                             std::string const& case_id,
                             double tolerance
                                 = std::numeric_limits<double>::epsilon())
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "test case: " << case_id << std::endl;
        std::stringstream sstr;
        sstr << "svgs/" << case_id << ".svg";
#ifdef TEST_WITH_SVG
        to_svg(geometry1, geometry2, sstr.str());
#endif
#endif

        Geometry1 rg1(geometry1);
        bg::reverse<Geometry1>(rg1);

        Geometry2 rg2(geometry2);
        bg::reverse<Geometry2>(rg2);

        typedef typename bg::tag_cast
            <
                Geometry1, bg::linear_tag
            >::type tag1_type;

        typedef typename bg::tag_cast
            <
                Geometry2, bg::linear_tag
            >::type tag2_type;

        bool const are_linear
            = boost::is_same<tag1_type, bg::linear_tag>::value
            && boost::is_same<tag2_type, bg::linear_tag>::value;

        test_get_turns_ll_invariance<are_linear>::apply(geometry1, geometry2);
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl
                  << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
                  << std::endl << std::endl;
#endif
        test_get_turns_ll_invariance<are_linear>::apply(rg1, geometry2);

        base_test(geometry1, geometry2, mls_int1, mls_int2, case_id, tolerance);
        //        base_test(rg1, rg2, mls_int1, mls_int2);
        base_test_all(geometry1, geometry2);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }



    static inline void apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             MultiLineString const& mls_int,
                             std::string const& case_id,
                             double tolerance
                                 = std::numeric_limits<double>::epsilon())
    {
        apply(geometry1, geometry2, mls_int, mls_int, case_id, tolerance);
    }
};


#endif // BOOST_GEOMETRY_TEST_INTERSECTION_LINEAR_LINEAR_HPP
