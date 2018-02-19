// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2013, 2014, 2015, 2017.
// Modifications copyright (c) 2013-2017 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_RELATE_HPP
#define BOOST_GEOMETRY_TEST_RELATE_HPP

#include <geometry_test_common.hpp>

#include <boost/variant.hpp>

#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/algorithms/relate.hpp>
#include <boost/geometry/algorithms/relation.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <boost/geometry/io/wkt/read.hpp>

#include <boost/geometry/strategies/cartesian/point_in_box.hpp>
#include <boost/geometry/strategies/cartesian/box_in_box.hpp>
#include <boost/geometry/strategies/agnostic/point_in_box_by_side.hpp>

namespace bgdr = bg::detail::relate;

std::string transposed(std::string matrix)
{
    if ( !matrix.empty() )
    {
        std::swap(matrix[1], matrix[3]);
        std::swap(matrix[2], matrix[6]);
        std::swap(matrix[5], matrix[7]);
    }
    return matrix;
}

bool matrix_compare(std::string const& m1, std::string const& m2)
{
    BOOST_ASSERT(m1.size() == 9 && m2.size() == 9);
    for ( size_t i = 0 ; i < 9 ; ++i )
    {
        if ( m1[i] == '*' || m2[i] == '*' )
            continue;

        if ( m1[i] != m2[i] )
            return false;
    }
    return true;
}

bool matrix_compare(std::string const& m, std::string const& res1, std::string const& res2)
{
    return matrix_compare(m, res1)
        || ( !res2.empty() ? matrix_compare(m, res2) : false );
}

std::string matrix_format(std::string const& matrix1, std::string const& matrix2)
{
    return matrix1
         + ( !matrix2.empty() ? " || " : "" ) + matrix2;
}

template <typename M>
char get_ii(M const& m)
{
    using bg::detail::relate::interior;
    return m.template get<interior, interior>();
}

template <typename M>
char get_ee(M const& m)
{
    using bg::detail::relate::exterior;
    return m.template get<exterior, exterior>();
}

void check_mask()
{
    bg::de9im::mask m1("");
    bg::de9im::mask m2("TTT");
    bg::de9im::mask m3("000111222");
    bg::de9im::mask m4("000111222FFFF");
    bg::de9im::mask m5(std::string(""));
    bg::de9im::mask m6(std::string("TTT"));
    bg::de9im::mask m7(std::string("000111222"));
    bg::de9im::mask m8(std::string("000111222FFFF"));

    using bg::detail::relate::interior;
    using bg::detail::relate::exterior;

    BOOST_CHECK(get_ii(m1) == '*' && get_ee(m1) == '*');
    BOOST_CHECK(get_ii(m2) == 'T' && get_ee(m2) == '*');
    BOOST_CHECK(get_ii(m3) == '0' && get_ee(m3) == '2');
    BOOST_CHECK(get_ii(m4) == '0' && get_ee(m4) == '2');
    BOOST_CHECK(get_ii(m5) == '*' && get_ee(m5) == '*');
    BOOST_CHECK(get_ii(m6) == 'T' && get_ee(m6) == '*');
    BOOST_CHECK(get_ii(m7) == '0' && get_ee(m7) == '2');
    BOOST_CHECK(get_ii(m8) == '0' && get_ee(m8) == '2');
}

template <typename Geometry1, typename Geometry2>
void check_geometry(Geometry1 const& geometry1,
                    Geometry2 const& geometry2,
                    std::string const& wkt1,
                    std::string const& wkt2,
                    std::string const& expected1,
                    std::string const& expected2 = std::string())
{
    boost::variant<Geometry1> variant1 = geometry1;
    boost::variant<Geometry2> variant2 = geometry2;

    {
        std::string res_str = bg::relation(geometry1, geometry2).str();
        bool ok = matrix_compare(res_str, expected1, expected2);
        BOOST_CHECK_MESSAGE(ok,
            "relate: " << wkt1
            << " and " << wkt2
            << " -> Expected: " << matrix_format(expected1, expected2)
            << " detected: " << res_str);

        typedef typename bg::strategy::relate::services::default_strategy
            <
                Geometry1, Geometry2
            >::type strategy_type;
        std::string res_str0 = bg::relation(geometry1, geometry2, strategy_type()).str();
        BOOST_CHECK(res_str == res_str0);

        // test variants
        boost::variant<Geometry1> v1 = geometry1;
        boost::variant<Geometry2> v2 = geometry2;
        std::string res_str1 = bg::relation(geometry1, variant2).str();
        std::string res_str2 = bg::relation(variant1, geometry2).str();
        std::string res_str3 = bg::relation(variant1, variant2).str();
        BOOST_CHECK(res_str == res_str1);
        BOOST_CHECK(res_str == res_str2);
        BOOST_CHECK(res_str == res_str3);
    }

    // changed sequence of geometries - transposed result
    {
        std::string res_str = bg::relation(geometry2, geometry1).str();
        std::string expected1_tr = transposed(expected1);
        std::string expected2_tr = transposed(expected2);
        bool ok = matrix_compare(res_str, expected1_tr, expected2_tr);
        BOOST_CHECK_MESSAGE(ok,
            "relate: " << wkt2
            << " and " << wkt1
            << " -> Expected: " << matrix_format(expected1_tr, expected2_tr)
            << " detected: " << res_str);
    }

    if ( expected2.empty() )
    {
        {
            bool result = bg::relate(geometry1, geometry2, bg::de9im::mask(expected1));
            // TODO: SHOULD BE !interrupted - CHECK THIS!
            BOOST_CHECK_MESSAGE(result, 
                "relate: " << wkt1
                << " and " << wkt2
                << " -> Expected: " << expected1);

            typedef typename bg::strategy::relate::services::default_strategy
                <
                    Geometry1, Geometry2
                >::type strategy_type;
            bool result0 = bg::relate(geometry1, geometry2, bg::de9im::mask(expected1), strategy_type());
            BOOST_CHECK(result == result0);

            // test variants
            bool result1 = bg::relate(geometry1, variant2, bg::de9im::mask(expected1));
            bool result2 = bg::relate(variant1, geometry2, bg::de9im::mask(expected1));
            bool result3 = bg::relate(variant1, variant2, bg::de9im::mask(expected1));
            BOOST_CHECK(result == result1);
            BOOST_CHECK(result == result2);
            BOOST_CHECK(result == result3);
        }

        if ( BOOST_GEOMETRY_CONDITION((
                bg::detail::relate::interruption_enabled<Geometry1, Geometry2>::value )) )
        {
            // brake the expected output
            std::string expected_interrupt = expected1;
            bool changed = false;
            BOOST_FOREACH(char & c, expected_interrupt)
            {
                if ( c >= '0' && c <= '9' )
                {
                    if ( c == '0' )
                        c = 'F';
                    else
                        --c;

                    changed = true;
                }
            }

            if ( changed )
            {
                bool result = bg::relate(geometry1, geometry2, bg::de9im::mask(expected_interrupt));
                // TODO: SHOULD BE interrupted - CHECK THIS!
                BOOST_CHECK_MESSAGE(!result,
                    "relate: " << wkt1
                    << " and " << wkt2
                    << " -> Expected interrupt for:" << expected_interrupt);
            }
        }
    }
}

template <typename Geometry1, typename Geometry2>
void test_geometry(std::string const& wkt1,
                   std::string const& wkt2,
                   std::string const& expected1,
                   std::string const& expected2 = std::string())
{
    Geometry1 geometry1;
    Geometry2 geometry2;
    bg::read_wkt(wkt1, geometry1);
    bg::read_wkt(wkt2, geometry2);
    check_geometry(geometry1, geometry2, wkt1, wkt2, expected1, expected2);
}

#endif // BOOST_GEOMETRY_TEST_RELATE_HPP
