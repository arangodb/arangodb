// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "common.hpp"

#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/geometry/algorithms/expand.hpp>

int test_main(int, char*[])
{
    geom g;

    bg::expand(g.b, g.pt, bg::strategy::expand::cartesian_point());
    bg::expand(g.b, g.pt, bg::strategy::expand::spherical_point());
    bg::expand(g.b, g.b, bg::strategy::expand::cartesian_box());
    bg::expand(g.b, g.b, bg::strategy::expand::spherical_box());
    bg::expand(g.b, g.s, bg::strategy::expand::cartesian_segment());
    bg::expand(g.b, g.s, bg::strategy::expand::spherical_segment<>());
    bg::expand(g.b, g.s, bg::strategy::expand::geographic_segment<>());

    bg::envelope(g.pt, g.b, bg::strategy::envelope::cartesian_point());
    bg::envelope(g.pt, g.b, bg::strategy::envelope::spherical_point());
    bg::envelope(g.b, g.b, bg::strategy::envelope::cartesian_box());
    bg::envelope(g.b, g.b, bg::strategy::envelope::spherical_box());
    bg::envelope(g.s, g.b, bg::strategy::envelope::cartesian_segment<>());
    bg::envelope(g.s, g.b, bg::strategy::envelope::spherical_segment<>());
    bg::envelope(g.s, g.b, bg::strategy::envelope::geographic_segment<>());
    bg::envelope(g.ls, g.b, bg::strategy::envelope::cartesian<>());
    bg::envelope(g.ls, g.b, bg::strategy::envelope::spherical<>());
    bg::envelope(g.ls, g.b, bg::strategy::envelope::geographic<>());
    bg::envelope(g.r, g.b, bg::strategy::envelope::cartesian<>());
    bg::envelope(g.r, g.b, bg::strategy::envelope::spherical<>());
    bg::envelope(g.r, g.b, bg::strategy::envelope::geographic<>());
    bg::envelope(g.po, g.b, bg::strategy::envelope::cartesian<>());
    bg::envelope(g.po, g.b, bg::strategy::envelope::spherical<>());
    bg::envelope(g.po, g.b, bg::strategy::envelope::geographic<>());
    bg::envelope(g.mls, g.b, bg::strategy::envelope::cartesian<>());
    bg::envelope(g.mls, g.b, bg::strategy::envelope::spherical<>());
    bg::envelope(g.mls, g.b, bg::strategy::envelope::geographic<>());
    bg::envelope(g.mpo, g.b, bg::strategy::envelope::cartesian<>());
    bg::envelope(g.mpo, g.b, bg::strategy::envelope::spherical<>());
    bg::envelope(g.mpo, g.b, bg::strategy::envelope::geographic<>());
    bg::envelope(g.mpt, g.b, bg::strategy::envelope::cartesian_multipoint());
    bg::envelope(g.mpt, g.b, bg::strategy::envelope::spherical_multipoint());

    return 0;
}
