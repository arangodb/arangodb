// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014, 2015.
// Modifications copyright (c) 2014-2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <deque>
#include <vector>

#include <boost/concept/requires.hpp>
#include <geometry_test_common.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/append.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>
#include <boost/geometry/geometries/register/linestring.hpp>
#include <boost/variant/variant.hpp>

#include <test_common/test_point.hpp>
#include <test_geometries/wrapped_boost_array.hpp>


BOOST_GEOMETRY_REGISTER_LINESTRING_TEMPLATED(std::vector)
BOOST_GEOMETRY_REGISTER_LINESTRING_TEMPLATED(std::deque)


template <bool EnableAll>
struct do_checks
{
    template <typename G>
    static inline void apply(G const& geometry,
                             std::size_t size1,
                             std::size_t = 0,
                             std::size_t = 0)
    {
        BOOST_CHECK_EQUAL(bg::num_points(geometry), size1);
    }
};


template<>
struct do_checks<true>
{
    template <typename G>
    static inline void apply(G const& geometry,
                             std::size_t size1,
                             std::size_t size2 = 0,
                             std::size_t size3 = 0)
    {
        do_checks<false>::apply(geometry, size1);
        BOOST_CHECK_EQUAL(bg::num_points(geometry[0]), size2);
        BOOST_CHECK_EQUAL(bg::num_points(geometry[1]), size3);
    }
};



template <bool HasMultiIndex, bool IsVariant>
struct test_geometry
{
    template <typename G>
    static inline void apply(G& geometry, bool check)
    {
        typedef typename bg::point_type<G>::type P;
        typedef do_checks<HasMultiIndex && !IsVariant> checks;

        bg::append(geometry, bg::make_zero<P>(), -1, 0);
        if (check)
        {
            checks::apply(geometry, 1u, 1u, 0u);
        }

        // Append a range
        std::vector<P> v;
        v.push_back(bg::make_zero<P>());
        v.push_back(bg::make_zero<P>());
        bg::append(geometry, v, -1, 1);

        if (check)
        {
            checks::apply(geometry, 3u, 1u, 2u);
        }

        bg::clear(geometry);

        if (check)
        {
            do_checks<false>::apply(geometry, 0u);
        }
    }
};



template <typename G>
void test_geometry_and_variant(bool check = true)
{
    G geometry;
    boost::variant<G> variant_geometry = G();
    test_geometry<false, false>::apply(geometry, check);
    test_geometry<false, true>::apply(variant_geometry, check);
}


template <typename MG>
void test_multigeometry_and_variant(bool check = true)
{
    typedef typename boost::range_value<MG>::type G;

    G geometry;
    MG multigeometry;
    bg::traits::push_back<MG>::apply(multigeometry, geometry);
    bg::traits::push_back<MG>::apply(multigeometry, geometry);

    boost::variant<MG> variant_multigeometry = multigeometry;
    test_geometry<true, false>::apply(multigeometry, check);
    test_geometry<true, true>::apply(variant_multigeometry, check);
}


template <typename P>
void test_all()
{
    test_geometry_and_variant<P>(false);
    test_geometry_and_variant<bg::model::box<P> >(false);
    test_geometry_and_variant<bg::model::segment<P> >(false);
    test_geometry_and_variant<bg::model::linestring<P> >();
    test_geometry_and_variant<bg::model::ring<P> >();
    test_geometry_and_variant<bg::model::polygon<P> >();
    test_geometry_and_variant<bg::model::multi_point<P> >();
    test_multigeometry_and_variant
        <
            bg::model::multi_linestring<bg::model::linestring<P> >
        >();
    test_multigeometry_and_variant
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >();

    test_geometry_and_variant<std::vector<P> >();
    test_geometry_and_variant<std::deque<P> >();
}

int test_main(int, char* [])
{
    test_all<test::test_point>();
    test_all<bg::model::point<int, 2, bg::cs::cartesian> >();
    test_all<bg::model::point<float, 2, bg::cs::cartesian> >();
    test_all<bg::model::point<double, 2, bg::cs::cartesian> >();

#ifdef HAVE_TTMATH
    test_all<bg::model::point<ttmath_big, 2, bg::cs::cartesian> >();
#endif

    return 0;
}
