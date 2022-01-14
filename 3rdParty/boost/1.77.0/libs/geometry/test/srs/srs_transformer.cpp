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
#include <boost/geometry/srs/projection.hpp>
#include <boost/geometry/srs/transformation.hpp>
#include <boost/geometry/strategies/transform/srs_transformer.hpp>

#include <boost/geometry/io/wkt/read.hpp>

#include "check_geometry.hpp"


int test_main(int, char*[])
{
    using namespace boost::geometry;
    using namespace boost::geometry::model;
    using namespace boost::geometry::srs;
    using namespace bg::strategy::transform;

    typedef point<double, 2, cs::geographic<degree> > point_ll;
    typedef point<double, 2, cs::cartesian> point_xy;
    //typedef polygon<point_ll> polygon_ll;
    //typedef polygon<point_xy> polygon_xy;

    {
        point_ll pt_ll(1, 1);
        point_ll pt_ll2(0, 0);
        point_xy pt_xy(0, 0);
        point_xy pt_xy2(0, 0);

        srs_forward_transformer<projection<> > strategy_pf = proj4("+proj=tmerc +ellps=WGS84 +units=m");
        srs_inverse_transformer<projection<> > strategy_pi = proj4("+proj=tmerc +ellps=WGS84 +units=m");
        srs_forward_transformer<transformation<> > strategy_tf(proj4("+proj=tmerc +ellps=WGS84 +units=m"),
                                                               proj4("+proj=tmerc +ellps=clrk66 +units=m"));
        srs_inverse_transformer<transformation<> > strategy_ti(proj4("+proj=tmerc +ellps=WGS84 +units=m"),
                                                               proj4("+proj=tmerc +ellps=clrk66 +units=m"));

        bg::transform(pt_ll, pt_xy, strategy_pf);        
        test::check_geometry(pt_xy, "POINT(111308.33561309829 110591.34223734379)", 0.0001);
        
        bg::transform(pt_xy, pt_ll2, strategy_pi);
        test::check_geometry(pt_ll2, "POINT(1 1)", 0.0001);

        bg::transform(pt_xy, pt_xy2, strategy_tf);
        test::check_geometry(pt_xy2, "POINT(111309.54843459482 110584.27813586517)", 0.0001);

        bg::transform(pt_xy2, pt_xy, strategy_ti);
        test::check_geometry(pt_xy, "POINT(111308.33561309829 110591.34223734379)", 0.0001);
    }

    {
        srs_forward_transformer<projection<> > strategy_pf = epsg(2000);
        srs_inverse_transformer<projection<> > strategy_pi = epsg(2000);
        srs_forward_transformer<transformation<> > strategy_tf(epsg(2000), epsg(2001));
        srs_inverse_transformer<transformation<> > strategy_ti(epsg(2000), epsg(2001));
    }

    {
        using namespace bg::srs::spar;

        srs_forward_transformer
            <
                projection<parameters<proj_tmerc, ellps_wgs84> >
            > strategy_pf;
        srs_forward_transformer
            <
                projection<parameters<proj_tmerc, ellps_wgs84> >
            > strategy_pi;
        srs_forward_transformer
            <
                transformation
                    <
                        parameters<proj_tmerc, ellps_wgs84>,
                        parameters<proj_tmerc, ellps_clrk66>
                    >
            > strategy_tf;
        srs_forward_transformer
            <
                transformation
                    <
                        parameters<proj_tmerc, ellps_wgs84>,
                        parameters<proj_tmerc, ellps_clrk66>
                    >
            > strategy_ti;
    }

    {
        srs_forward_transformer<projection<static_epsg<2000> > > strategy_pf;
        srs_forward_transformer<projection<static_epsg<2000> > > strategy_pi;
        srs_forward_transformer<transformation<static_epsg<2000>, static_epsg<2001> > > strategy_tf;
        srs_forward_transformer<transformation<static_epsg<2000>, static_epsg<2001> > > strategy_ti;
    }

    return 0;
}
