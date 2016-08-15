// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014, 2015.
// Modifications copyright (c) 2014-2015 Oracle and/or its affiliates.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

#include "test_within.hpp"


#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>

template <typename P>
void test_l_l()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<ls, ls>("LINESTRING(0 0, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 3 2)", true);

    test_geometry<ls, ls>("LINESTRING(0 0, 1 1, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 3 2)", true);
    test_geometry<ls, ls>("LINESTRING(3 2, 2 2, 1 1, 0 0)", "LINESTRING(0 0, 2 2, 3 2)", true);
    test_geometry<ls, ls>("LINESTRING(0 0, 1 1, 2 2, 3 2)", "LINESTRING(3 2, 2 2, 0 0)", true);
    test_geometry<ls, ls>("LINESTRING(3 2, 2 2, 1 1, 0 0)", "LINESTRING(3 2, 2 2, 0 0)", true);

    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", true);
    test_geometry<ls, ls>("LINESTRING(3 2, 2 2, 1 1)", "LINESTRING(0 0, 2 2, 4 2)", true);
    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 2)", "LINESTRING(4 2, 2 2, 0 0)", true);
    test_geometry<ls, ls>("LINESTRING(3 2, 2 2, 1 1)", "LINESTRING(4 2, 2 2, 0 0)", true);

    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 3)", "LINESTRING(0 0, 2 2, 4 2)", false);
    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 2, 3 3)", "LINESTRING(0 0, 2 2, 4 2)", false);
    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 1)", "LINESTRING(0 0, 2 2, 4 2)", false);
    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 2, 3 1)", "LINESTRING(0 0, 2 2, 4 2)", false);

    test_geometry<ls, ls>("LINESTRING(0 1, 1 1, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", false);
    test_geometry<ls, ls>("LINESTRING(0 1, 0 0, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", false);
    test_geometry<ls, ls>("LINESTRING(1 0, 1 1, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", false);
    test_geometry<ls, ls>("LINESTRING(1 0, 0 0, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", false);

    // duplicated points
    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 2 2)", "LINESTRING(0 0, 2 2, 4 2)", true);
    test_geometry<ls, ls>("LINESTRING(1 1, 1 1, 2 2)", "LINESTRING(0 0, 2 2, 4 2)", true);

    test_geometry<ls, ls>("LINESTRING(0 0, 0 0, 0 0, 1 1, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 2 2, 3 3)",
                          "LINESTRING(0 0, 2 2, 4 4)", true);

    // invalid linestrings
//    test_geometry<ls, ls>("LINESTRING(0 0)", "LINESTRING(0 0)", false);
//    test_geometry<ls, ls>("LINESTRING(1 1)", "LINESTRING(0 0, 2 2)", true);
//    test_geometry<ls, ls>("LINESTRING(0 0)", "LINESTRING(0 0, 2 2)", false);
//    test_geometry<ls, ls>("LINESTRING(0 0, 1 1)", "LINESTRING(0 0)", false);

    // spikes
    // FOR NOW DISABLED

    /*test_geometry<ls, ls>("LINESTRING(0 0,5 0,3 0,6 0)", "LINESTRING(0 0,6 0)", true);

    test_geometry<ls, ls>("LINESTRING(0 0,2 2,3 3,1 1)", "LINESTRING(0 0,3 3,6 3)", true);
    test_geometry<ls, ls>("LINESTRING(0 0,3 3,6 3)", "LINESTRING(0 0,2 2,3 3,1 1)", false);
    test_geometry<ls, ls>("LINESTRING(0 0,2 2,3 3,1 1)", "LINESTRING(0 0,4 4,6 3)", true);
    test_geometry<ls, ls>("LINESTRING(0 0,4 4,6 3)", "LINESTRING(0 0,2 2,3 3,1 1)", false);
    
    test_geometry<ls, ls>("LINESTRING(0 0,2 2,3 3,1 1,5 3)", "LINESTRING(0 0,3 3,6 3)", false);*/

    test_geometry<ls, mls>("LINESTRING(1 1, 2 2)", "MULTILINESTRING((0 0, 2 2),(3 3, 4 4))", true);

    test_geometry<mls, ls>("MULTILINESTRING((0 0, 2 2),(3 3, 4 4))", "LINESTRING(0 0, 5 5)", true);

    test_geometry<mls, mls>("MULTILINESTRING((1 1, 2 2),(3 3, 4 4))", "MULTILINESTRING((1 1, 2 2),(2 2, 5 5))", true);
}

template <typename P>
void test_all()
{
    test_l_l<P>();
}

int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();

#if defined(HAVE_TTMATH)
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
