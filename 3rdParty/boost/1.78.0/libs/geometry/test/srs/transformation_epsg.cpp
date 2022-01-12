// Boost.Geometry

// Copyright (c) 2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/srs/epsg.hpp>
#include <boost/geometry/srs/transformation.hpp>

#include "check_geometry.hpp"

template <typename T>
void test_issue_657()
{
    using namespace boost::geometry;
    using namespace boost::geometry::model;
    using namespace boost::geometry::srs;

    typedef model::point<T, 2, bg::cs::cartesian> point_car;
    typedef model::point<T, 2, cs::geographic<bg::degree> > point_geo;
    
    transformation<> tr1((bg::srs::epsg(4326)),
                         (bg::srs::proj4(
                             "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 "
                             "+y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs")));
    transformation<> tr2((bg::srs::epsg(4326)),
                         (bg::srs::epsg(3785)));
    transformation<bg::srs::static_epsg<4326>,
                   bg::srs::static_epsg<3785> > tr3;

    point_geo pt(-114.7399212, 36.0160698);
    point_car pt_out(-12772789.6016, 4302832.77709);
    point_car pt_out1, pt_out2, pt_out3;

    tr1.forward(pt, pt_out1);
    tr2.forward(pt, pt_out2);
    tr3.forward(pt, pt_out3);

    test::check_geometry(pt_out1, pt_out, 0.001);
    test::check_geometry(pt_out2, pt_out, 0.001);
    test::check_geometry(pt_out3, pt_out, 0.001);
}

int test_main(int, char*[])
{
    test_issue_657<double>();
    
    return 0;
}
