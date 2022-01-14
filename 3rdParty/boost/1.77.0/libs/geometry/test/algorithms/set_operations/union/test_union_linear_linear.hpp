// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2020, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#ifndef BOOST_GEOMETRY_TEST_UNION_LINEAR_LINEAR_HPP
#define BOOST_GEOMETRY_TEST_UNION_LINEAR_LINEAR_HPP

#include <limits>

#include <boost/range/value_type.hpp>

#include <boost/geometry/geometry.hpp>
#include "../test_set_ops_linear_linear.hpp"
#include <from_wkt.hpp>
#include <to_svg.hpp>


//==================================================================
//==================================================================
// union of (linear) geometries
//==================================================================
//==================================================================

template <typename Geometry1, typename Geometry2, typename MultiLineString>
inline void check_result(Geometry1 const& geometry1,
                         Geometry2 const& geometry2,
                         MultiLineString const& mls_output,
                         MultiLineString const& mls_union,
                         std::string const& case_id,
                         double tolerance)
{
    BOOST_CHECK_MESSAGE( equals::apply(mls_union, mls_output, tolerance),
                         "case id: " << case_id
                         << ", union L/L: " << bg::wkt(geometry1)
                         << " " << bg::wkt(geometry2)
                         << " -> Expected: " << bg::wkt(mls_union)
                         << " computed: " << bg::wkt(mls_output) );
}

template
<
    typename Geometry1, typename Geometry2,
    typename MultiLineString
>
class test_union_of_geometries
{
private:
    static inline void base_test(Geometry1 const& geometry1,
                                 Geometry2 const& geometry2,
                                 MultiLineString const& mls_union1,
                                 MultiLineString const& mls_union2,
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

        // Check strategy passed explicitly
        typedef typename bg::strategy::relate::services::default_strategy
            <
                Geometry1, Geometry2
            >::type strategy_type;
        bg::union_(geometry1, geometry2, mls_output, strategy_type());

        check_result(geometry1, geometry2, mls_output, mls_union1, case_id, tolerance);

        // Check normal behaviour
        bg::clear(mls_output);
        bg::union_(geometry1, geometry2, mls_output);

        check_result(geometry1, geometry2, mls_output, mls_union1, case_id, tolerance);

        set_operation_output("union", case_id,
                             geometry1, geometry2, mls_output);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "Geometry #1: " << bg::wkt(geometry1) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(geometry2) << std::endl;
        std::cout << "union : " << bg::wkt(mls_output) << std::endl;
        std::cout << "expected union : " << bg::wkt(mls_union1)
                  << std::endl;
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
            bg::union_(geometry1, geometry2, ls_vector_output);
            bg::union_(geometry1, geometry2, ls_deque_output);

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(mls_union1, ls_vector_output, tolerance));

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(mls_union1, ls_deque_output, tolerance));

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "Done!" << std::endl << std::endl;
#endif
        }

        // check the union where the order of the two
        // geometries is reversed
        bg::clear(mls_output);
        bg::union_(geometry2, geometry1, mls_output);

        check_result(geometry1, geometry2, mls_output, mls_union2, case_id, tolerance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "Geometry #1: " << bg::wkt(geometry2) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(geometry1) << std::endl;
        std::cout << "union : " << bg::wkt(mls_output) << std::endl;
        std::cout << "expected union : " << bg::wkt(mls_union2)
                  << std::endl;
        std::cout << std::endl;
        std::cout << "************************************" << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }


public:
    static inline void apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             MultiLineString const& mls_union1,
                             MultiLineString const& mls_union2,
                             std::string const& case_id,
                             double tolerance
                                 = std::numeric_limits<double>::epsilon())
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "test case: " << case_id << std::endl;
        std::stringstream sstr;
        sstr << "svgs/" << case_id << ".svg";
        to_svg(geometry1, geometry2, sstr.str());
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

        base_test(geometry1, geometry2, mls_union1, mls_union2,
                  case_id, tolerance, true);
        //        base_test(geometry1, rg2, mls_sym_diff);
        //        base_test(rg1, geometry2, mls_sym_diff);
        base_test(rg1, rg2, mls_union1, mls_union2, case_id, tolerance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }


    static inline void apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             MultiLineString const& mls_union,
                             std::string const& case_id,
                             double tolerance
                                 = std::numeric_limits<double>::epsilon())
    {
        apply(geometry1, geometry2, mls_union, mls_union, case_id, tolerance);
    }
};


#endif // BOOST_GEOMETRY_TEST_UNION1_HPP
