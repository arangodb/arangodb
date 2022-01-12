// Boost.Geometry
// Unit Test

// Copyright (c) 2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/visit.hpp>
#include <boost/geometry/geometries/adapted/boost_any.hpp>
#include <boost/geometry/geometries/adapted/boost_variant.hpp>
#include <boost/geometry/geometries/adapted/boost_variant2.hpp>
#include <boost/geometry/geometries/adapted/std_any.hpp>
#include <boost/geometry/geometries/adapted/std_variant.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/util/type_traits.hpp>


using point_t = bg::model::point<double, 2, bg::cs::cartesian>;
using linestring_t = bg::model::linestring<point_t>;
using polygon_t = bg::model::polygon<point_t>;


namespace boost { namespace geometry { namespace traits
{

template <>
struct geometry_types<boost::any>
{
    typedef util::type_sequence<point_t, linestring_t, polygon_t> type;
};

#ifndef BOOST_NO_CXX17_HDR_ANY

template <>
struct geometry_types<std::any>
{
    typedef util::type_sequence<point_t, linestring_t, polygon_t> type;
};

#endif // BOOST_NO_CXX17_HDR_ANY

}}}


template <typename DynamicGeometry>
void test_all()
{
    DynamicGeometry dg = point_t(1, 2);
    bg::detail::visit([](auto g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<decltype(g)>::value);
    }, dg);
    bg::detail::visit([](auto const g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<decltype(g)>::value);
    }, dg);
    bg::detail::visit([](auto & g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<std::remove_reference_t<decltype(g)>>::value);
    }, dg);
    bg::detail::visit([](auto const& g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g)>>::value);
    }, dg);

    DynamicGeometry const cdg = point_t(3, 4);
    bg::detail::visit([](auto g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<decltype(g)>::value);
    }, cdg);
    bg::detail::visit([](auto const g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<decltype(g)>::value);
    }, cdg);
    bg::detail::visit([](auto & g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g)>>::value);
    }, cdg);
    bg::detail::visit([](auto const& g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g)>>::value);
    }, cdg);


    bg::detail::visit([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(! std::is_rvalue_reference<decltype(g)>::value);
    }, dg);
    bg::detail::visit([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(! std::is_rvalue_reference<decltype(g)>::value);
    }, cdg);
    bg::detail::visit([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g)>::value);
    }, std::move(dg));
    bg::detail::visit([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g)>::value);
    }, std::move(cdg));
    bg::detail::visit([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g)>::value);
    }, DynamicGeometry{ point_t(1, 2) });


    bg::detail::visit([](auto && g1, auto && g2)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g1)>::value);
        BOOST_CHECK(bg::util::is_point<decltype(g2)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<std::remove_reference_t<decltype(g1)>>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g2)>>::value);
        BOOST_STATIC_ASSERT(! std::is_rvalue_reference<decltype(g1)>::value);
        BOOST_STATIC_ASSERT(! std::is_rvalue_reference<decltype(g2)>::value);
    }, dg, cdg);

    bg::detail::visit([](auto && g1, auto && g2)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g1)>::value);
        BOOST_CHECK(bg::util::is_point<decltype(g2)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<std::remove_reference_t<decltype(g1)>>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g2)>>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g1)>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g2)>::value);
    }, std::move(dg), std::move(cdg));

    bg::detail::visit([](auto && g1, auto && g2)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g2)>::value);
        BOOST_CHECK(bg::util::is_point<decltype(g1)>::value);
        BOOST_STATIC_ASSERT(!std::is_const<std::remove_reference_t<decltype(g2)>>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g1)>>::value);
        BOOST_STATIC_ASSERT(!std::is_rvalue_reference<decltype(g2)>::value);
        BOOST_STATIC_ASSERT(!std::is_rvalue_reference<decltype(g1)>::value);
    }, cdg, dg);

    bg::detail::visit([](auto && g1, auto && g2)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g2)>::value);
        BOOST_CHECK(bg::util::is_point<decltype(g1)>::value);
        BOOST_STATIC_ASSERT(!std::is_const<std::remove_reference_t<decltype(g2)>>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g1)>>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g2)>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g1)>::value);
    }, std::move(cdg), std::move(dg));


    bg::model::geometry_collection<DynamicGeometry> gc = { DynamicGeometry{ point_t(1, 2) } };
    bg::model::geometry_collection<DynamicGeometry> const cgc = { DynamicGeometry{ point_t(1, 2) } };

    bg::detail::visit_breadth_first([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(! std::is_rvalue_reference<decltype(g)>::value);
        return true;
    }, gc);

    bg::detail::visit_breadth_first([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(! std::is_rvalue_reference<decltype(g)>::value);
        return true;
    }, cgc);

    bg::detail::visit_breadth_first([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(! std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g)>::value);
        return true;
    }, std::move(gc));

    bg::detail::visit_breadth_first([](auto && g)
    {
        BOOST_CHECK(bg::util::is_point<decltype(g)>::value);
        BOOST_STATIC_ASSERT(std::is_const<std::remove_reference_t<decltype(g)>>::value);
        BOOST_STATIC_ASSERT(std::is_rvalue_reference<decltype(g)>::value);
        return true;
    }, std::move(cgc));
}

int test_main(int, char* [])
{
    test_all<boost::any>();
    test_all<boost::variant<point_t, linestring_t, polygon_t>>();
    test_all<boost::variant2::variant<point_t, linestring_t, polygon_t>>();

#ifndef BOOST_NO_CXX17_HDR_ANY
    test_all<std::any>();
#endif
#ifndef BOOST_NO_CXX17_HDR_VARIANT
    test_all<std::variant<point_t, linestring_t, polygon_t>>();
#endif

    return 0;
}
