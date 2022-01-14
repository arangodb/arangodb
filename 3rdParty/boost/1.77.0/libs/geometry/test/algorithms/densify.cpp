// Boost.Geometry
// Unit Test

// Copyright (c) 2017-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#include <geometry_test_common.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/algorithms/densify.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>

#include <boost/geometry/iterators/segment_iterator.hpp>

#include <boost/geometry/strategies/cartesian/densify.hpp>
#include <boost/geometry/strategies/cartesian/distance_pythagoras.hpp>
#include <boost/geometry/strategies/geographic/densify.hpp>
#include <boost/geometry/strategies/geographic/distance.hpp>
#include <boost/geometry/strategies/spherical/densify.hpp>
#include <boost/geometry/strategies/spherical/distance_haversine.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>


struct check_lengths
{
    template <typename G, typename S>
    void operator()(G const& g, G const& o, S const& s) const
    {
        double d1 = bg::length(g, s);
        double d2 = bg::length(o, s);

        BOOST_CHECK_CLOSE(d1, d2, 0.0001);
    }
};

struct check_perimeters
{
    template <typename G, typename S>
    void operator()(G const& g, G const& o, S const& s) const
    {
        double d1 = bg::perimeter(g, s);
        double d2 = bg::perimeter(o, s);

        BOOST_CHECK_CLOSE(d1, d2, 0.0001);
    }
};

template <typename G, typename DistS>
double inline shortest_length(G const& g, DistS const& dist_s)
{
    double min_len = (std::numeric_limits<double>::max)();
    for (bg::segment_iterator<G const> it = bg::segments_begin(g);
            it != bg::segments_end(g); ++it)
    {
        double len = bg::length(*it, dist_s);
        min_len = (std::min)(min_len, len);
    }
    return min_len;
}

template <typename G, typename DistS>
double inline greatest_length(G const& o, DistS const& dist_s)
{
    double max_len = 0.0;
    for (bg::segment_iterator<G const> it = bg::segments_begin(o);
            it != bg::segments_end(o); ++it)
    {
        double len = bg::length(*it, dist_s);
        max_len = (std::max)(max_len, len);
    }
    return max_len;
}

template <typename G, typename CSTag = typename bg::cs_tag<G>::type>
struct cs_data
{};

template <typename G>
struct cs_data<G, bg::cartesian_tag>
{
    bg::strategy::densify::cartesian<> compl_s;
    bg::strategy::distance::pythagoras<> dist_s;
};

template <typename G>
struct cs_data<G, bg::spherical_equatorial_tag>
{
    cs_data()
        : model(6378137.0)
        , compl_s(model)
        , dist_s(6378137.0)
    {}

    bg::srs::sphere<double> model;
    bg::strategy::densify::spherical<> compl_s;
    bg::strategy::distance::haversine<double> dist_s;
};

template <typename G>
struct cs_data<G, bg::geographic_tag>
{
    cs_data()
        : model(6378137.0, 6356752.3142451793)
        , compl_s(model)
        , dist_s(model)
    {}

    bg::srs::spheroid<double> model;
    bg::strategy::densify::geographic<> compl_s;
    bg::strategy::distance::geographic<> dist_s;
};

template <typename G, typename DistS, typename Check>
inline void check_result(G const& g, G const& o, double max_distance,
                         DistS const& dist_s, Check const& check)
{
    // geometry was indeed densified
    std::size_t g_count = bg::num_points(g);
    std::size_t o_count = bg::num_points(o);
    BOOST_CHECK(g_count < o_count);

    // all segments have lengths smaller or equal to max_distance
    double gr_len = greatest_length(o, dist_s);
    // NOTE: Currently geographic strategies can generate segments that have
    //       lengths slightly greater than max_distance. In order to change
    //       this the generation of new points should e.g. be recursive with
    //       stop condition comparing the current distance calculated by
    //       inverse strategy.
    // NOTE: Closeness value tweaked for Andoyer
    bool is_close = (gr_len - max_distance) / (std::max)(gr_len, max_distance) < 0.0001;
    BOOST_CHECK(gr_len <= max_distance || is_close);

    // the overall length or perimeter didn't change
    check(g, o, dist_s);
}

template <typename G, typename Check>
inline void test_geometry(std::string const& wkt, Check const& check)
{
    cs_data<G> d;

    G g;
    bg::read_wkt(wkt, g);

    {
        bg::default_strategy def_s;
        double max_distance = shortest_length(g, def_s) / 3.0;

        G o;
        bg::densify(g, o, max_distance);

        check_result(g, o, max_distance, def_s, check);
    }

    {
        double max_distance = shortest_length(g, d.dist_s) / 3.0;

        G o;
        bg::densify(g, o, max_distance, d.compl_s);

        check_result(g, o, max_distance, d.dist_s, check);
    }
}

template <typename G>
inline void test_linear(std::string const& wkt)
{
    test_geometry<G>(wkt, check_lengths());
}

template <typename G>
inline void test_areal(std::string const& wkt)
{
    test_geometry<G>(wkt, check_perimeters());
}

template <typename P>
void test_all()
{
    typedef bg::model::linestring<P> ls_t;
    typedef bg::model::multi_linestring<ls_t> mls_t;

    typedef bg::model::ring<P> ring_t;
    typedef bg::model::polygon<P> poly_t;
    typedef bg::model::multi_polygon<poly_t> mpoly_t;

    typedef bg::model::ring<P, true, false> oring_t;
    typedef bg::model::polygon<P, true, false> opoly_t;
    typedef bg::model::multi_polygon<opoly_t> ompoly_t;

    test_linear<ls_t>("LINESTRING(4 -4, 4 -1)");
    test_linear<ls_t>("LINESTRING(4 4, 4 1)");
    test_linear<ls_t>("LINESTRING(0 0, 180 0)");
    test_linear<ls_t>("LINESTRING(1 1, -179 -1)");

    test_linear<ls_t>("LINESTRING(1 1, 2 2, 4 2)");
    test_linear<mls_t>("MULTILINESTRING((1 1, 2 2),(2 2, 4 2))");

    test_areal<ring_t>("POLYGON((1 1, 1 2, 2 2, 1 1))");
    test_areal<poly_t>("POLYGON((1 1, 1 4, 4 4, 4 1, 1 1),(1 1, 2 2, 2 3, 1 1))");
    test_areal<mpoly_t>("MULTIPOLYGON(((1 1, 1 4, 4 4, 4 1, 1 1),(1 1, 2 2, 2 3, 1 1)),((4 4, 5 5, 5 4, 4 4)))");
    
    test_areal<oring_t>("POLYGON((1 1, 1 2, 2 2))");
    test_areal<opoly_t>("POLYGON((1 1, 1 4, 4 4, 4 1),(1 1, 2 2, 2 3))");
    test_areal<ompoly_t>("MULTIPOLYGON(((1 1, 1 4, 4 4, 4 1),(1 1, 2 2, 2 3)),((4 4, 5 5, 5 4)))");

    test_areal<ring_t>("POLYGON((0 0,0 40,40 40,40 0,0 0))");
    test_areal<oring_t>("POLYGON((0 0,0 40,40 40,40 0))");
}

int test_main(int, char* [])
{
    test_all< bg::model::point<double, 2, bg::cs::cartesian> >();
    test_all< bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_all< bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();
    
    return 0;
}
