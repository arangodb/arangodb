// Boost.Geometry
// Unit Test

// Copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/srs/epsg.hpp>
#include <boost/geometry/srs/esri.hpp>
#include <boost/geometry/srs/iau2000.hpp>
#include <boost/geometry/srs/projection.hpp>

#include "check_geometry.hpp"

int test_main(int, char*[])
{
    using namespace boost::geometry;
    using namespace boost::geometry::model;
    using namespace boost::geometry::srs;

    typedef point<double, 2, cs::geographic<degree> > point_ll;
    typedef point<double, 2, cs::cartesian> point_xy;

    {
        point_ll pt_ll(1, 1);
        point_ll pt_ll2(0, 0);
        point_xy pt_xy(0, 0);

        projection<> prj = epsg(2000);
        
        prj.forward(pt_ll, pt_xy);
        test::check_geometry(pt_xy, "POINT(9413505.3284665551 237337.74515944949)", 0.001);

        prj.inverse(pt_xy, pt_ll2);
        // TODO: investigate this wierd result
        test::check_geometry(pt_ll2, "POINT(-2.4463131191981073 1.5066638962045082)", 0.001);

        projection<> prj2 = esri(37001);
        projection<> prj3 = iau2000(19900);
    }

    {
        using namespace boost::geometry::srs::dpar;

        point_ll pt_ll(1, 1);
        point_ll pt_ll2(0, 0);
        point_xy pt_xy(0, 0);

        projection<> prj = parameters<>(proj_tmerc)(ellps_wgs84)(units_m);

        prj.forward(pt_ll, pt_xy);
        test::check_geometry(pt_xy, "POINT(111308.33561309829 110591.34223734379)", 0.001);

        prj.inverse(pt_xy, pt_ll2);
        test::check_geometry(pt_ll2, "POINT(1 1)", 0.001);
    }

    return 0;
}
