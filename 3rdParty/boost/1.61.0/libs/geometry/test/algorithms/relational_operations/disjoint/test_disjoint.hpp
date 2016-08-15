// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_DISJOINT_HPP
#define BOOST_GEOMETRY_TEST_DISJOINT_HPP

#include <iostream>
#include <string>
#include <boost/variant/variant.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/disjoint.hpp>
#include <boost/geometry/io/wkt/read.hpp>


template <typename G1, typename G2>
void check_disjoint(std::string const& id,
                    G1 const& g1,
                    G2 const& g2,
                    bool expected)
{
    bool detected = bg::disjoint(g1, g2);
    BOOST_CHECK_MESSAGE(detected == expected,
        "disjoint: " << id
        << " -> Expected: " << expected
        << " detected: " << detected);
}

template <typename G1, typename G2>
void test_disjoint(std::string const& id,
            std::string const& wkt1,
            std::string const& wkt2, bool expected)
{
    G1 g1;
    bg::read_wkt(wkt1, g1);

    G2 g2;
    bg::read_wkt(wkt2, g2);

    boost::variant<G1> v1(g1);
    boost::variant<G2> v2(g2);

    check_disjoint(id, g1, g2, expected);
    check_disjoint(id, v1, g2, expected);
    check_disjoint(id, g1, v2, expected);
    check_disjoint(id, v1, v2, expected);
}


#endif // BOOST_GEOMETRY_TEST_DISJOINT_HPP
