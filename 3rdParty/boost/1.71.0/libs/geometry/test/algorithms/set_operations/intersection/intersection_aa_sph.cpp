// Boost.Geometry
// Unit Test

// Copyright (c) 2016, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_intersection.hpp"

template <typename P>
void test_all()
{
    typedef bg::model::polygon<P> polygon;
    
    // https://svn.boost.org/trac/boost/ticket/11789
    test_one<polygon, polygon, polygon>("poly_poly_se_ticket_11789",
        "POLYGON((-4.5726431789237223 52.142932977753595, \
                  -4.5743166242433153 52.143359442355219, \
                  -4.5739141406075410 52.143957260988416, \
                  -4.5722406991324354 52.143530796430468, \
                  -4.5726431789237223 52.142932977753595))",
        "POLYGON((-4.5714644516017975 52.143819810922480, \
                  -4.5670821923630358 52.143819810922480, \
                  -4.5670821923630358 52.143649055226163, \
                  -4.5714644516017975 52.143649055226163, \
                  -4.5714644516017975 52.143819810922480))",
        0, 0, 0.0);
}

int test_main(int, char* [])
{
    test_all<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();

    return 0;
}
