// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2014 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2014 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014.
// Modifications copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <strategies/test_projected_point.hpp>


template <typename P1, typename P2>
void test_all_2d_ax()
{
    typedef bg::strategy::distance::detail::projected_point_ax<> strategy_type;
    typedef bg::strategy::distance::detail::projected_point_ax
        <
            void,
            bg::strategy::distance::comparable::pythagoras<>
        > comparable_strategy_type;

    typedef typename strategy_type::result_type<P1, P2>::type result_type;

    strategy_type strategy;
    comparable_strategy_type comparable_strategy;
    boost::ignore_unused(strategy, comparable_strategy);

    test_2d<P1, P2>("POINT(1 1)", "POINT(0 0)", "POINT(2 3)",
                    result_type(0, 0.27735203958327),
                    result_type(0, 0.27735203958327 * 0.27735203958327),
                    strategy, comparable_strategy);

    test_2d<P1, P2>("POINT(2 2)", "POINT(1 4)", "POINT(4 1)",
                    result_type(0, 0.5 * sqrt(2.0)),
                    result_type(0, 0.5),
                    strategy, comparable_strategy);

    test_2d<P1, P2>("POINT(6 1)", "POINT(1 4)", "POINT(4 1)",
                    result_type(sqrt(2.0), sqrt(2.0)),
                    result_type(2.0, 2.0),
                    strategy, comparable_strategy);

    test_2d<P1, P2>("POINT(1 6)", "POINT(1 4)", "POINT(4 1)",
                    result_type(sqrt(2.0), sqrt(2.0)),
                    result_type(2.0, 2.0),
                    strategy, comparable_strategy);
}

template <typename P>
void test_all_2d_ax()
{
    test_all_2d_ax<P, bg::model::point<int, 2, bg::cs::cartesian> >();
    test_all_2d_ax<P, bg::model::point<float, 2, bg::cs::cartesian> >();
    test_all_2d_ax<P, bg::model::point<double, 2, bg::cs::cartesian> >();
    test_all_2d_ax<P, bg::model::point<long double, 2, bg::cs::cartesian> >();
}

int test_main(int, char* [])
{
    test_all_2d_ax<bg::model::point<int, 2, bg::cs::cartesian> >();
    test_all_2d_ax<bg::model::point<float, 2, bg::cs::cartesian> >();
    test_all_2d_ax<bg::model::point<double, 2, bg::cs::cartesian> >();

#if defined(HAVE_TTMATH)
    test_all_2d_ax
        <
            bg::model::point<ttmath_big, 2, bg::cs::cartesian>,
            bg::model::point<ttmath_big, 2, bg::cs::cartesian>
        >();
#endif


    return 0;
}
