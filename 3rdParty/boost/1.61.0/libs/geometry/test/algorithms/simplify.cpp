// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iterator>


#include <algorithms/test_simplify.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <test_geometries/wrapped_boost_array.hpp>
#include <test_common/test_point.hpp>

// #define TEST_PULL89
#ifdef TEST_PULL89
#include <boost/geometry/strategies/cartesian/distance_projected_point_ax.hpp>
#endif


#ifdef TEST_PULL89
template <typename Geometry, typename T>
void test_with_ax(std::string const& wkt,
        std::string const& expected,
        T const& adt,
        T const& xdt)
{
    typedef typename bg::point_type<Geometry>::type point_type;
    typedef bg::strategy::distance::detail::projected_point_ax<> ax_type;
    typedef typename bg::strategy::distance::services::return_type
    <
        bg::strategy::distance::detail::projected_point_ax<>,
        point_type,
        point_type
    >::type return_type;

    typedef bg::strategy::distance::detail::projected_point_ax_less
    <
        return_type
    > comparator_type;

    typedef bg::strategy::simplify::detail::douglas_peucker
    <
        point_type,
        bg::strategy::distance::detail::projected_point_ax<>,
        comparator_type
    > dp_ax;

    return_type max_distance(adt, xdt);
    comparator_type comparator(max_distance);
    dp_ax strategy(comparator);

    test_geometry<Geometry>(wkt, expected, max_distance, strategy);
}
#endif


template <typename P>
void test_all()
{
    test_geometry<bg::model::linestring<P> >(
        "LINESTRING(0 0,5 5,10 10)",
        "LINESTRING(0 0,10 10)", 1.0);

    test_geometry<bg::model::linestring<P> >(
        "LINESTRING(0 0, 5 5, 6 5, 10 10)",
        "LINESTRING(0 0,10 10)", 1.0);

    test_geometry<bg::model::linestring<P> >(
        "LINESTRING(0 0,5 5,7 5,10 10)",
        "LINESTRING(0 0,5 5,7 5,10 10)", 1.0);

    // Lightning-form which fails for Douglas-Peucker
    test_geometry<bg::model::linestring<P> >(
        "LINESTRING(0 0,120 6,80 10,200 0)",
        "LINESTRING(0 0,120 6,80 10,200 0)", 7);
    // Same which reordered coordinates
    test_geometry<bg::model::linestring<P> >(
        "LINESTRING(0 0,80 10,120 6,200 0)",
        "LINESTRING(0 0,80 10,200 0)", 7);

    // Mail 2013-10-07, real-life test, piece of River Leine
    // PostGIS returns exactly the same result
    test_geometry<bg::model::linestring<P> >(
         "LINESTRING(4293586 3290439,4293568 3290340,4293566 3290332,4293570 3290244,4293576 3290192"
                   ",4293785 3289660,4293832 3289597,4293879 3289564,4293937 3289545,4294130 3289558"
                   ",4294204 3289553,4294240 3289539,4294301 3289479,4294317 3289420,4294311 3289353"
                   ",4294276 3289302,4293870 3289045,4293795 3288978,4293713 3288879,4293669 3288767"
                   ",4293654 3288652,4293657 3288563,4293690 3288452,4293761 3288360,4293914 3288215"
                   ",4293953 3288142,4293960 3288044,4293951 3287961,4293913 3287875,4293708 3287628"
                   ",4293658 3287542,4293633 3287459,4293630 3287383,4293651 3287323,4293697 3287271"
                   ",4293880 3287128,4293930 3287045,4293938 3286977,4293931 3286901,4293785 3286525"
                   ",4293775 3286426,4293786 3286358,4293821 3286294,4294072 3286076,4294134 3285986)",
          "LINESTRING(4293586 3290439,4293785 3289660,4294317 3289420,4293654 3288652,4293960 3288044"
                  ",4293633 3287459,4293786 3286358,4294134 3285986)", 250);

    /* TODO fix this
    test_geometry<test::wrapped_boost_array<P, 10> >(
        "LINESTRING(0 0,5 5,7 5,10 10)",
        "LINESTRING(0 0,5 5,7 5,10 10)", 1.0);
    */

    test_geometry<bg::model::ring<P> >(
        "POLYGON((4 0,8 2,8 7,4 9,0 7,0 2,2 1,4 0))",
        "POLYGON((4 0,8 2,8 7,4 9,0 7,0 2,4 0))", 1.0);

    test_geometry<bg::model::polygon<P> >(
        "POLYGON((4 0,8 2,8 7,4 9,0 7,0 2,2 1,4 0))",
        "POLYGON((4 0,8 2,8 7,4 9,0 7,0 2,4 0))", 1.0);

    test_geometry<bg::model::polygon<P> >(
        "POLYGON((4 0,8 2,8 7,4 9,0 7,0 2,2 1,4 0),(7 3,7 6,1 6,1 3,4 3,7 3))",
        "POLYGON((4 0,8 2,8 7,4 9,0 7,0 2,4 0),(7 3,7 6,1 6,1 3,7 3))", 1.0);

/*

Above can be checked in PostGIS by:

select astext(ST_Simplify(geomfromtext('LINESTRING(0 0, 5 5, 10 10)'),1.0)) as simplified
union all select astext(ST_Simplify(geomfromtext('LINESTRING(0 0, 5 5, 6 5, 10 10)'),1.0))
etc
*/

    {
        // Test with explicit strategy

        typedef bg::strategy::simplify::douglas_peucker
        <
            P,
            bg::strategy::distance::projected_point<double>
        > dp;

        test_geometry<bg::model::linestring<P> >(
            "LINESTRING(0 0,5 5,10 10)",
            "LINESTRING(0 0,10 10)", 1.0, dp());
    }


    // POINT: check compilation
    test_geometry<P>(
        "POINT(0 0)",
        "POINT(0 0)", 1.0);


    // RING: check compilation and behaviour
    test_geometry<bg::model::ring<P> >(
        "POLYGON((4 0,8 2,8 7,4 9,0 7,0 2,2 1,4 0))",
        "POLYGON((4 0,8 2,8 7,4 9,0 7,0 2,4 0))", 1.0);


#ifdef TEST_PULL89
    test_with_ax<bg::model::linestring<P> >(
        "LINESTRING(0 0,120 6,80 10,200 0)",
        "LINESTRING(0 0,80 10,200 0)", 10, 7);
#endif
}

template <typename P>
void test_zigzag()
{
    static const std::string zigzag = "LINESTRING(0 10,1 7,1 9,2 6,2 7,3 4,3 5,5 3,4 5,6 2,6 3,9 1,7 3,10 1,9 2,12 1,10 2,13 1,11 2,14 1,12 2,16 1,14 2,17 3,15 3,18 4,16 4,19 5,17 5,20 6,18 6,21 8,19 7,21 9,19 8,21 10,19 9,21 11,19 10,20 13,19 11)";

    static const std::string expected100 = "LINESTRING(0 10,3 4,5 3,4 5,6 2,9 1,7 3,10 1,9 2,16 1,14 2,17 3,15 3,18 4,16 4,19 5,17 5,21 8,19 7,21 9,19 8,21 10,19 9,21 11,19 10,20 13,19 11)";
    static const std::string expected150 = "LINESTRING(0 10,6 2,16 1,14 2,21 8,19 7,21 9,19 8,21 10,19 9,20 13,19 11)";
    static const std::string expected200 = "LINESTRING(0 10,6 2,16 1,14 2,21 8,19 7,20 13,19 11)";
    static const std::string expected225 = "LINESTRING(0 10,6 2,16 1,21 8,19 11)";
    test_geometry<bg::model::linestring<P> >(zigzag, expected100, 1.0001);
    test_geometry<bg::model::linestring<P> >(zigzag, expected150, 1.5001);
    test_geometry<bg::model::linestring<P> >(zigzag, expected200, 2.0001);
    test_geometry<bg::model::linestring<P> >(zigzag, expected225, 2.25); // should be larger than sqrt(5)=2.236

#ifdef TEST_PULL89
    // This should work (results might vary but should have LESS points then expected above
    // Small xtd, larger adt,
    test_with_ax<bg::model::linestring<P> >(zigzag, expected100, 1.0001, 1.0001);
    test_with_ax<bg::model::linestring<P> >(zigzag, expected150, 1.5001, 1.0001);
    test_with_ax<bg::model::linestring<P> >(zigzag, expected200, 2.0001, 1.0001);
    test_with_ax<bg::model::linestring<P> >(zigzag, expected225, 2.25, 1.0001);
#endif

}


template <typename P>
void test_3d()
{
    test_geometry<bg::model::linestring<P> >(
        "LINESTRING(0 0 0,1 1 1,2 2 0)",
        "LINESTRING(0 0 0,2 2 0)", 1.0001);
    test_geometry<bg::model::linestring<P> >(
        "LINESTRING(0 0 0,1 1 1,2 2 0)",
        "LINESTRING(0 0 0,1 1 1,2 2 0)", 0.9999);
}


template <typename P>
void test_spherical()
{
    test_geometry<bg::model::linestring<P> >(
        "LINESTRING(4.1 52.1,4.2 52.2,4.3 52.3)",
        "LINESTRING(4.1 52.1,4.3 52.3)", 0.01);
}


int test_main(int, char* [])
{
    // Integer compiles, but simplify-process fails (due to distances)
    //test_all<bg::model::d2::point_xy<int> >();

    test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();

    test_3d<bg::model::point<double, 3, bg::cs::cartesian> >();

    test_spherical<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();

    test_zigzag<bg::model::d2::point_xy<double> >();

#if defined(HAVE_TTMATH)
    test_all<bg::model::d2::point_xy<ttmath_big> >();
    test_spherical<bg::model::point<ttmath_big, 2, bg::cs::spherical_equatorial<bg::degree> > >();
#endif



    return 0;
}
