// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "common.hpp"

// These includes are required for the following code to compile.
// This is probably wrong.
#include <boost/geometry/algorithms/covered_by.hpp>
#include <boost/geometry/algorithms/disjoint.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/algorithms/intersects.hpp>

#include <boost/geometry/index/rtree.hpp>

#include <vector>

namespace bgi = boost::geometry::index;

template
<
    typename VG, typename QG,
    typename VTag = typename bg::tag<VG>::type,
    typename QTag = typename bg::tag<QG>::type
>
struct call_query
{
    template <typename Rtree, typename Res>
    static inline void apply(Rtree const& , Res const& )
    {}
};

template <typename VG, typename QG>
struct call_query<VG, QG, bg::box_tag, bg::point_tag>
{
    template <typename Rtree>
    static inline void apply(Rtree const& rtree, QG const& qg)
    {
        std::vector<VG> res;
        rtree.query(bgi::intersects(qg), std::back_inserter(res));
    }
};

template <typename G, typename P>
inline void rtree_test(G const& g, P const& p)
{
    {
        bgi::rtree<G, P> rtree;
    }

    std::vector<G> de2(100, g);

    bgi::rtree<G, P> rtree(de2, p);
    rtree.insert(g);
    rtree.remove(g);
    rtree.count(g);

    call_query<G, geom::point>::apply(rtree, geom::point(0, 0));
}

int test_main(int, char*[])
{
    geom g;

    rtree_test(g.pt, bgi::parameters<bgi::linear<4>, bg::strategy::index::cartesian<> >());
    rtree_test(g.pt, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::cartesian<> >());
    rtree_test(g.pt, bgi::parameters<bgi::rstar<4>, bg::strategy::index::cartesian<> >());
    rtree_test(g.b, bgi::parameters<bgi::linear<4>, bg::strategy::index::cartesian<> >());
    rtree_test(g.b, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::cartesian<> >());
    rtree_test(g.b, bgi::parameters<bgi::rstar<4>, bg::strategy::index::cartesian<> >());
    rtree_test(g.s, bgi::parameters<bgi::linear<4>, bg::strategy::index::cartesian<> >());
    rtree_test(g.s, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::cartesian<> >());
    rtree_test(g.s, bgi::parameters<bgi::rstar<4>, bg::strategy::index::cartesian<> >());

    rtree_test(g.pt, bgi::parameters<bgi::linear<4>, bg::strategy::index::spherical<> >());
    rtree_test(g.pt, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::spherical<> >());
    rtree_test(g.pt, bgi::parameters<bgi::rstar<4>, bg::strategy::index::spherical<> >());
    rtree_test(g.b, bgi::parameters<bgi::linear<4>, bg::strategy::index::spherical<> >());
    rtree_test(g.b, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::spherical<> >());
    rtree_test(g.b, bgi::parameters<bgi::rstar<4>, bg::strategy::index::spherical<> >());
    rtree_test(g.s, bgi::parameters<bgi::linear<4>, bg::strategy::index::spherical<> >());
    rtree_test(g.s, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::spherical<> >());
    rtree_test(g.s, bgi::parameters<bgi::rstar<4>, bg::strategy::index::spherical<> >());

    rtree_test(g.pt, bgi::parameters<bgi::linear<4>, bg::strategy::index::geographic<> >());
    rtree_test(g.pt, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::geographic<> >());
    rtree_test(g.pt, bgi::parameters<bgi::rstar<4>, bg::strategy::index::geographic<> >());
    rtree_test(g.b, bgi::parameters<bgi::linear<4>, bg::strategy::index::geographic<> >());
    rtree_test(g.b, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::geographic<> >());
    rtree_test(g.b, bgi::parameters<bgi::rstar<4>, bg::strategy::index::geographic<> >());
    rtree_test(g.s, bgi::parameters<bgi::linear<4>, bg::strategy::index::geographic<> >());
    rtree_test(g.s, bgi::parameters<bgi::quadratic<4>, bg::strategy::index::geographic<> >());
    rtree_test(g.s, bgi::parameters<bgi::rstar<4>, bg::strategy::index::geographic<> >());

    return 0;
}
