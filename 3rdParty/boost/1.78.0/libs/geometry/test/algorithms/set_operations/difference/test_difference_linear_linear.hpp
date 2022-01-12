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

//! Contains (optional) settings such as tolerance
//! and to skip some test configurations
struct ut_settings
{
    ut_settings() = default;

    // Make it backwards compatible by a non-explicit constructor
    // such that just tolerance is also accepted
    ut_settings(const double t) : tolerance(t) {}

    double tolerance{std::numeric_limits<double>::epsilon()};
    bool test_normal_normal{true};
    bool test_normal_reverse{true};
    bool test_reverse_normal{true};
    bool test_reverse_reverse{true};
};


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
                         ut_settings const& settings = {})
{
    BOOST_CHECK_MESSAGE( equals::apply(mls_diff, mls_output, settings.tolerance),
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
                                 MultiLineString const& expected,
                                 std::string const& case_id,
                                 ut_settings const& settings,
                                 bool test_vector_and_deque = true,
                                 bool reverse_output_for_checking = false)
    {
        static bool vector_deque_already_tested = false;

        typedef typename boost::range_value<MultiLineString>::type LineString;
        typedef std::vector<LineString> linestring_vector;
        typedef std::deque<LineString> linestring_deque;

        MultiLineString detected;


        // Check strategy passed explicitly
        typedef typename bg::strategy::relate::services::default_strategy
            <
                Geometry1, Geometry2
            >::type strategy_type;
        bg::difference(geometry1, geometry2, detected, strategy_type());

        if (reverse_output_for_checking)
        {
            bg::reverse(detected);
        }

#ifdef TEST_WITH_SVG
        to_svg(geometry1, geometry2, detected, "difference_" + case_id);
#endif

        check_result(geometry1, geometry2, detected, expected, case_id + "_s",
                     settings);

        // Check algorithm without strategy
        bg::clear(detected);
        bg::difference(geometry1, geometry2, detected);

        if (reverse_output_for_checking)
        {
            bg::reverse(detected);
        }

        check_result(geometry1, geometry2, detected, expected, case_id, settings);
        
        set_operation_output("difference", case_id,
                             geometry1, geometry2, detected);

        if (! vector_deque_already_tested && test_vector_and_deque)
        {
            vector_deque_already_tested = true;
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << std::endl;
            std::cout << "Testing with vector and deque as output container..."
                      << std::endl;
#endif
            linestring_vector ls_vector_output;
            bg::difference(geometry1, geometry2, ls_vector_output);

            linestring_deque ls_deque_output;
            bg::difference(geometry1, geometry2, ls_deque_output);

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(expected, ls_vector_output, settings.tolerance));

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(expected, ls_deque_output, settings.tolerance));

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
                             MultiLineString const& expected,
                             std::string const& case_id,
                             ut_settings const& settings = {})
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "test case: " << case_id << std::endl;
#endif

        // For testing the reverse cases
        Geometry1 rg1(geometry1);
        bg::reverse<Geometry1>(rg1);

        Geometry2 rg2(geometry2);
        bg::reverse<Geometry2>(rg2);

        test_get_turns_ll_invariance<>::apply(geometry1, geometry2);
        test_get_turns_ll_invariance<>::apply(rg1, geometry2);

        if (settings.test_normal_normal)
        {
            base_test(geometry1, geometry2, expected, case_id, settings);
        }
        if (settings.test_normal_reverse)
        {
            base_test(geometry1, rg2, expected, case_id + "-nr", settings,
                      false);
        }
        if (settings.test_reverse_normal)
        {
            base_test(rg1, geometry2, expected, case_id + "-rn", settings,
                      false, true);
        }
        if (settings.test_reverse_reverse)
        {
            base_test(rg1, rg2, expected, case_id + "-rr", settings,
                      false, true);
        }

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }
};


#endif // BOOST_GEOMETRY_TEST_DIFFERENCE_LINEAR_LINEAR_HPP
