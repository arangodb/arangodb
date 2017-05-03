// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#ifndef BOOST_GEOMETRY_TEST_SYM_DIFFERENCE_LINEAR_LINEAR_HPP
#define BOOST_GEOMETRY_TEST_SYM_DIFFERENCE_LINEAR_LINEAR_HPP

#include <limits>

#include <boost/geometry/geometry.hpp>
#include "../test_set_ops_linear_linear.hpp"
#include <from_wkt.hpp>
#include <to_svg.hpp>


//==================================================================
//==================================================================
// symmetric difference of (linear) geometries
//==================================================================
//==================================================================

template
<
    typename Geometry1, typename Geometry2,
    typename MultiLineString
>
class test_sym_difference_of_geometries
{
private:
    static inline void base_test(Geometry1 const& geometry1,
                                 Geometry2 const& geometry2,
                                 MultiLineString const& mls_sym_diff,
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

        bg::sym_difference(geometry1, geometry2, mls_output);

        BOOST_CHECK_MESSAGE( equals::apply(mls_sym_diff, mls_output, tolerance),
                             "case id: " << case_id
                             << ", sym diff L/L: " << bg::wkt(geometry1)
                             << " " << bg::wkt(geometry2)
                             << " -> Expected: " << bg::wkt(mls_sym_diff)
                             << " computed: " << bg::wkt(mls_output) );


        set_operation_output("sym_difference", case_id,
                             geometry1, geometry2, mls_output);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "Geometry #1: " << bg::wkt(geometry1) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(geometry2) << std::endl;
        std::cout << "sym_difference : " << bg::wkt(mls_output) << std::endl;
        std::cout << "expected sym_difference : " << bg::wkt(mls_sym_diff)
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
            bg::sym_difference(geometry1, geometry2, ls_vector_output);
            bg::sym_difference(geometry1, geometry2, ls_deque_output);

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(mls_sym_diff, ls_vector_output, tolerance));

            BOOST_CHECK(multilinestring_equals
                        <
                            false
                        >::apply(mls_sym_diff, ls_deque_output, tolerance));

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "Done!" << std::endl << std::endl;
#endif
        }

        // check the symmetric difference where the order of the two
        // geometries is reversed
        bg::clear(mls_output);
        bg::sym_difference(geometry2, geometry1, mls_output);

        BOOST_CHECK_MESSAGE( equals::apply(mls_sym_diff, mls_output, tolerance),
                             "case id: " << case_id
                             << ", sym diff L/L: " << bg::wkt(geometry2)
                             << " " << bg::wkt(geometry1)
                             << " -> Expected: " << bg::wkt(mls_sym_diff)
                             << " computed: " << bg::wkt(mls_output) );

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "Geometry #1: " << bg::wkt(geometry2) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(geometry1) << std::endl;
        std::cout << "sym_difference : " << bg::wkt(mls_output) << std::endl;
        std::cout << "expected sym_difference : " << bg::wkt(mls_sym_diff)
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
                             MultiLineString const& mls_sym_diff,
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

        base_test(geometry1, geometry2, mls_sym_diff, case_id, tolerance, true);
        //        base_test(geometry1, rg2, mls_sym_diff);
        //        base_test(rg1, geometry2, mls_sym_diff);
        base_test(rg1, rg2, mls_sym_diff, case_id, tolerance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }
};


#endif // BOOST_GEOMETRY_TEST_SYM_DIFFERENCE_LINEAR_LINEAR_HPP
