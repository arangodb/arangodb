// Boost.Geometry
// Unit Test

// Copyright (c) 2018, 2021, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#include <geometry_test_common.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/algorithms/line_interpolate.hpp>
#include <boost/geometry/algorithms/length.hpp>

#include <boost/geometry/iterators/segment_iterator.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

template <typename P, typename Tag = typename bg::tag<P>::type>
struct check_points: bg::not_implemented<Tag>
{};

template <typename P>
struct check_points<P, bg::point_tag>
{
    static void apply(P const& p0, P const& p1)
    {
        double p00 = bg::get<0>(p0);
        double p10 = bg::get<0>(p1);

        BOOST_CHECK_CLOSE(p00, p10, 0.001);

        double p01 = bg::get<1>(p0);
        double p11 = bg::get<1>(p1);

        BOOST_CHECK_CLOSE(p01, p11, 0.001);
    }
};

template <typename P>
struct check_points<P, bg::multi_point_tag>
{
    template <typename Range>
    static void apply(Range const& r0, Range const& r1)
    {

        typedef typename boost::range_iterator<Range const>::type iterator_t;
        typedef typename boost::range_value<Range const>::type point_t;

        std::size_t count0 = boost::size(r0);
        std::size_t count1 = boost::size(r1);

        BOOST_CHECK_MESSAGE(count0 == count1, bg::wkt(r0) << " != " << bg::wkt(r1));
        
        if (count0 == count1)
        {
            for (iterator_t it0 = boost::begin(r0), it1 = boost::begin(r1);
                 it0 < boost::end(r0); it0++, it1++)
            {
                check_points<point_t>::apply(*it0, *it1);
            }
        }
    }
};

template <typename G, typename P, typename S>
inline void test(std::string const& wkt1,
                 double fraction,
                 std::string const& wkt2,
                 S str)
{
    G g;
    bg::read_wkt(wkt1, g);

    P o;
    bg::read_wkt(wkt2, o);

    P p1;
    bg::line_interpolate(g, fraction * bg::length(g), p1, str);
    check_points<P>::apply(p1, o);

}

template <typename G, typename P>
inline void test(std::string const& wkt1,
                 double fraction,
                 std::string const& wkt2)
{
    G g;
    bg::read_wkt(wkt1, g);

    P o;
    bg::read_wkt(wkt2, o);

    P p1;
    bg::line_interpolate(g, fraction * bg::length(g), p1);
    check_points<P>::apply(p1, o);
}

template <typename G, typename P>
inline void test_distance(std::string const& wkt1,
                          double distance,
                          std::string const& wkt2)
{
    G g;
    bg::read_wkt(wkt1, g);

    P o;
    bg::read_wkt(wkt2, o);

    P p1;
    bg::line_interpolate(g, distance, p1);
    check_points<P>::apply(p1, o);
}

template <typename G, typename P, typename S>
inline void test_distance(std::string const& wkt1,
                          double distance,
                          std::string const& wkt2,
                          S str)
{
    G g;
    bg::read_wkt(wkt1, g);

    P o;
    bg::read_wkt(wkt2, o);

    P p1;
    bg::line_interpolate(g, distance, p1, str);
    check_points<P>::apply(p1, o);
}

std::string const s = "SEGMENT(1 1, 2 2)";
std::string const l1 = "LINESTRING(1 1, 2 1, 2 2, 1 2, 1 3)";
std::string const l2 = "LINESTRING(0 2, 5 2, 5 1, 20 1)";
std::string const l00 = "LINESTRING()";
std::string const l01 = "LINESTRING(1 1)";
std::string const l02 = "LINESTRING(1 1, 1 1)";

void test_car_edge_cases()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> P;
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::linestring<P> LS;

    //negative input distance
    test_distance<LS,P>(l1, -1, "POINT(1 1)");
    test_distance<LS,MP>(l1, -1, "MULTIPOINT((1 1))");

    //input distance longer than total length
    test_distance<LS,P>(l1, 5, "POINT(1 3)");
    test_distance<LS,MP>(l1, 5, "MULTIPOINT((1 3))");

    //linestring with only one point
    test_distance<LS,P>(l01, 1, "POINT(1 1)");
    test_distance<LS,MP>(l01, 1, "MULTIPOINT((1 1))");

    //linestring with two same points
    test_distance<LS,P>(l02, 1, "POINT(1 1)");
    test_distance<LS,MP>(l02, 1, "MULTIPOINT((1 1))");

    //empty linestring
    try
    {
        test_distance<LS,P>(l00, 1, "POINT(1 1)");
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false, "A empty_input_exception should have been thrown" );
}

void test_car()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> P;
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::segment<P> S;
    typedef bg::model::linestring<P> LS;

    test<S,P>(s, 0,   "POINT(1 1)");
    test<S,P>(s, 0.5, "POINT(1.5 1.5)");
    test<S,P>(s, 1,   "POINT(2 2)");

    test<LS,P>(l1, 0,   "POINT(1 1)");
    test<LS,P>(l1, 0.1, "POINT(1.4 1)");
    test<LS,P>(l1, 0.2, "POINT(1.8 1)");
    test<LS,P>(l1, 0.3, "POINT(2 1.2)");
    test<LS,P>(l1, 0.4, "POINT(2 1.6)");
    test<LS,P>(l1, 0.5, "POINT(2 2)");
    test<LS,P>(l1, 0.6, "POINT(1.6 2)");
    test<LS,P>(l1, 0.7, "POINT(1.2 2)");
    test<LS,P>(l1, 0.8, "POINT(1 2.2)");
    test<LS,P>(l1, 0.9, "POINT(1 2.6)");
    test<LS,P>(l1, 1,   "POINT(1 3)");

    test<LS,MP>(l1, 0, "MULTIPOINT((1 1))");
    // The following type of test is not robust;
    // (1 3) could be missing due to floating point round off errors
    //test<LS,MP>(l1, 0.1, "MULTIPOINT((1.4 1)(1.8 1)(2 1.2)(2 1.6)(2 2)(1.6 2)\
    //                                (1.2 2)(1 2.2)(1 2.6)(1 3))");
    // Tests are more robust if you directly pass the distance
    test_distance<LS,MP>(l1, 0.4, "MULTIPOINT((1.4 1)(1.8 1)(2 1.2)(2 1.6)(2 2)(1.6 2)\
                                             (1.2 2)(1 2.2)(1 2.6)(1 3))");
    test<LS,MP>(l1, 0.09, "MULTIPOINT((1.36 1)(1.72 1)(2 1.08)(2 1.44)(2 1.8)\
                                      (1.84 2)(1.48 2)(1.12 2)(1 2.24)(1 2.6)(1 2.96))");
    test<LS,MP>(l1, 0.21, "MULTIPOINT((1.84 1)(2 1.68)(1.48 2)(1 2.36))");
    test<LS,MP>(l1, 0.4, "MULTIPOINT((2 1.6)(1 2.2))");
    test<LS,MP>(l1, 0.6, "MULTIPOINT((1.6 2))");
    test<LS,MP>(l1, 1, "MULTIPOINT((1 3))");
}

void test_sph()
{
    typedef bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > P;
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::segment<P> S;
    typedef bg::model::linestring<P> LS;

    test<S,P>(s, 0,   "POINT(1 1)");
    test<S,P>(s, 0.5, "POINT(1.4998857365615981 1.5000570914791198)");
    test<S,P>(s, 1,   "POINT(2 2)");

    test<LS,P>(l1, 0,   "POINT(1 1)");
    test<LS,P>(l1, 0.1, "POINT(1.39998476912905323 1.0000365473536286)");
    test<LS,P>(l1, 0.2, "POINT(1.79996953825810646 1.0000243679448551)");
    test<LS,P>(l1, 0.3, "POINT(2 1.1999238595669637)");
    test<LS,P>(l1, 0.4, "POINT(2 1.5998477098527744)");
    test<LS,P>(l1, 0.5, "POINT(2 1.9997715601390484)");
    test<LS,P>(l1, 0.6, "POINT(1.6000609543036084 2.0000730473928678)");
    test<LS,P>(l1, 0.7, "POINT(1.1998933176222553 2.0000486811516014)");
    test<LS,P>(l1, 0.8, "POINT(1 2.2001522994279883)");
    test<LS,P>(l1, 0.9, "POINT(1 2.6000761497139444)");
    test<LS,P>(l1, 1,   "POINT(1 3)");

    test<LS,MP>(l1, 0, "MULTIPOINT((1 1))");
    test<LS,MP>(l1, 0.09, "MULTIPOINT((1.35999 1.00004)(1.71997 1.00003)\
                                      (2 1.07995)(2 1.43988)(2 1.79981)(1.84016 2.00004)\
                                      (1.48001 2.00008)(1.11986 2.00003)(1 2.24014)\
                                      (1 2.60008)(1 2.96001))");
    test<LS,MP>(l1, 0.21, "MULTIPOINT((1.83997 1.00002)(2 1.67983)(1.48001 2.00008)(1 2.36012))");
    test<LS,MP>(l1, 0.4, "MULTIPOINT((2 1.5998477098527744)(1 2.2001522994279883))");
    test<LS,MP>(l1, 0.6, "MULTIPOINT((1.6000609543036084 2.0000730473928678))");
    test<LS,MP>(l1, 1, "MULTIPOINT((1 3))");

    test<LS,MP>(l2, 0.3, "MULTIPOINT((5.3014893312120446 1.0006787676128222)\
                                     (11.600850053156366 1.0085030143490989)\
                                     (17.9002174825842 1.0041514208039872))");
}

template <typename Strategy>
void test_sph(Strategy str)
{
    typedef bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > P;
    typedef bg::model::segment<P> S;

    test_distance<S,P>(s, 0,   "POINT(1 1)", str);
    test_distance<S,P>(s, 0.01, "POINT(1.4051065077123643 1.405268220524982)");
    test_distance<S,P>(s, 0.01, "POINT(1.0040505023484179 1.0040529633262307)", str);
    test_distance<S,P>(s, 1,   "POINT(1.4051065077123015 1.405268220524919)", str);
    test_distance<S,P>(s, 1,   "POINT(2 2)");
    test_distance<S,P>(s, 10,   "POINT(2 2)");
}

template <typename Strategy>
void test_geo(Strategy str)
{
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > P;
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::segment<P> S;
    typedef bg::model::linestring<P> LS;

    test<S,P>(s, 0,   "POINT(1 1)", str);
    test<S,P>(s, 0.5, "POINT(1.4998780900539985 1.5000558288006378)", str);
    test<S,P>(s, 1,   "POINT(2 2)", str);

    test<LS,P>(l1, 0,   "POINT(1 1)", str);
    test<LS,P>(l1, 0.1, "POINT(1.3986445638301882 1.0000367522730751)", str);
    test<LS,P>(l1, 0.2, "POINT(1.79728912766037641 1.0000247772611039)", str);
    test<LS,P>(l1, 0.3, "POINT(2 1.1972285554368427)", str);
    test<LS,P>(l1, 0.4, "POINT(2 1.598498298996567)", str);
    test<LS,P>(l1, 0.5, "POINT(2 1.9997664696834965)", str);
    test<LS,P>(l1, 0.6, "POINT(1.6013936980010324 2.0000734568388099)", str);
    test<LS,P>(l1, 0.7, "POINT(1.2025664628960846 2.0000494983098767)", str);
    test<LS,P>(l1, 0.8, "POINT(1 2.1974612279909937)", str);
    test<LS,P>(l1, 0.9, "POINT(1 2.5987263175375022)", str);
    test<LS,P>(l1, 1,   "POINT(1 3)", str);

    test<LS,MP>(l1, 0, "MULTIPOINT((1 1))", str);
    test<LS,MP>(l1, 0.11, "MULTIPOINT((1.43851 1.00004)(1.87702 1.00002)(2 1.31761)\
                (2 1.75901)(1.80081 2.00005)(1.3621 2.00007)\
                (1 2.07708)(1 2.51847)(1 2.95986))", str);
    test<LS,MP>(l1, 0.21, "MULTIPOINT((1.83716 1.00002)(2 1.67875)(1.48176 2.00008)\
                                      (1 2.35796))", str);
    test<LS,MP>(l1, 0.4, "MULTIPOINT((2 1.598498298996567)(1 2.1974612279909937))", str);
    test<LS,MP>(l1, 0.6, "MULTIPOINT((1.6013936980010324 2.0000734568388099))", str);
    test<LS,MP>(l1, 1, "MULTIPOINT((1 3))", str);

    test<LS,MP>(l2, 0.3, "MULTIPOINT((5.306157814 1.0006937303)\
                                     (11.60351281 1.0085614548123072)\
                                     (17.90073492 1.004178475142552))", str);
}

template <typename Strategy>
void test_geo_non_standard_spheroid(Strategy str)
{
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > P;
    typedef bg::model::segment<P> S;

    test<S,P>(s, 0,   "POINT(1 1)", str);
    test<S,P>(s, 0.5, "POINT(1.5127731436886724 1.5129021873759412)", str);
    test<S,P>(s, 1,   "POINT(2 2)", str);
}

int test_main(int, char* [])
{
    test_car();
    test_car_edge_cases();

    test_sph();
    test_sph(bg::strategy::line_interpolate::spherical<>(100));

    typedef bg::srs::spheroid<double> stype;

    test_geo(bg::strategy::line_interpolate::geographic<bg::strategy::andoyer>());
    test_geo(bg::strategy::line_interpolate::geographic<bg::strategy::thomas>());
    test_geo(bg::strategy::line_interpolate::geographic<bg::strategy::vincenty>());

    test_geo_non_standard_spheroid(bg::strategy::line_interpolate::geographic
                                   <bg::strategy::vincenty>(stype(5000000,6000000)));

    return 0;
}

