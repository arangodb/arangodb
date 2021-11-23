// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2020, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#ifndef BOOST_GEOMETRY_TEST_DIFFERENCE_LINEAR_LINEAR_HPP
#define BOOST_GEOMETRY_TEST_DIFFERENCE_LINEAR_LINEAR_HPP

#include <limits>

#include <boost/range/value_type.hpp>

#include <boost/geometry/geometry.hpp>
#include "../test_set_ops_linear_linear.hpp"
#include "../check_turn_less.hpp"
#include <from_wkt.hpp>
#include <to_svg.hpp>


//==================================================================
//==================================================================
// difference of (linear) geometries
//==================================================================
//==================================================================

template <typename Geometry1, typename Geometry2, typename MultiLineString>
inline void check_result(Geometry1 const& geometry1,
                         Geometry2 const& geometry2,
                         MultiLineString const& mls_output,
                         MultiLineString const& mls_diff,
                         std::string const& case_id,
                         double tolerance = std::numeric_limits<double>::epsilon())
{
    BOOST_CHECK_MESSAGE( equals::apply(mls_diff, mls_output, tolerance),
                         "case id: " << case_id
                         << ", difference L/L: " << bg::wkt(geometry1)
                         << " " << bg::wkt(geometry2)
                         << " -> Expected: " << bg::wkt(mls_diff)
                         << std::setprecision(20)
                         << " computed: " << bg::wkt(mls_output) );
}

template
<
    typename Geometry1, typename Geometry2,
    typename MultiLineString
>
class test_difference_of_geometries
{
private:
    static inline void base_test(Geometry1 const& geometry1,
                                 Geometry2 const& geometry2,
                                 MultiLineString const& mls_diff,
                                 std::string const& case_id,
                                 double tolerance,
                                 bool test_vector_and_deque = true,
                                 bool reverse_output_for_checking = false)
    {
        static bool vector_deque_already_tested = false;

        typedef typename boost::range_value<MultiLineString>::type LineString;
        typedef std::vector<LineString> linestring_vector;
        typedef std::deque<LineString> linestring_deque;

        MultiLineString mls_output;

        linestring_vector ls_vector_output;
        linestring_deque ls_deque_output;

        // Check strategy passed explicitly
        typedef typename bg::strategy::relate::services::default_strategy
            <
                Geometry1, Geometry2
            >::type strategy_type;
        bg::difference(geometry1, geometry2, mls_output, strategy_type());

        if (reverse_output_for_checking)
        {
            bg::reverse(mls_output);
        }

        check_result(geometry1, geometry2, mls_output, mls_diff, case_id, tolerance);

        // Check normal behaviour
        bg::clear(mls_output);
        bg::difference(geometry1, geometry2, mls_output);

        if ( reverse_output_for_checking ) 
        {
            bg::reverse(mls_output);
        }

        check_result(geometry1, geometry2, mls_output, mls_diff, case_id, tolerance);
        
        set_operation_output("difference", case_id,
                             geometry1, geometry2, mls_output);

        if ( !vector_deque_already_tested && test_vector_and_deque )
        {
            vector_deque_already_tested = true;
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << std::endl;
            std::cout << "Testing with vector and deque as output container..."
                      << std::endl;
#endif
            bg::difference(geometry1, geometry2, ls_vector_output);
            bg::difference(geometry1, geometry2, ls_deque_output);

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(mls_diff, ls_vector_output, tolerance));

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(mls_diff, ls_deque_output, tolerance));

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "Done!" << std::endl << std::endl;
#endif
        }

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "Geometry #1: " << bg::wkt(geometry1) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(geometry2) << std::endl;
        std::cout << "difference : " << bg::wkt(mls_output) << std::endl;
        std::cout << "expected difference : " << bg::wkt(mls_diff) << std::endl;
        std::cout << std::endl;
        std::cout << "************************************" << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
#endif

        check_turn_less::apply(geometry1, geometry2);
    }


public:
    static inline void apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             MultiLineString const& mls_diff,
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

        test_get_turns_ll_invariance<>::apply(geometry1, geometry2);
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl
                  << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
                  << std::endl << std::endl;
#endif
        test_get_turns_ll_invariance<>::apply(rg1, geometry2);

        base_test(geometry1, geometry2, mls_diff, case_id, tolerance);
        base_test(geometry1, rg2, mls_diff, case_id, tolerance, false);
        base_test(rg1, geometry2, mls_diff, case_id, tolerance, false, true);
        base_test(rg1, rg2, mls_diff, case_id, tolerance, false, true);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }
};


#endif // BOOST_GEOMETRY_TEST_DIFFERENCE_LINEAR_LINEAR_HPP
