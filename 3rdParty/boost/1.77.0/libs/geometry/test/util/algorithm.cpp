// Boost.Geometry

// Copyright (c) 2021, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/assign.hpp>

#include <boost/geometry/core/cs.hpp>

#include <boost/geometry/geometries/point.hpp>

#include <boost/geometry/util/algorithm.hpp>


void test_dimension(bg::util::index_constant<0>)
{
    bool called = false;
    bg::detail::for_each_index<0>([&](auto index) { called = true; });
    BOOST_CHECK(!called);
    BOOST_CHECK(bg::detail::all_indexes_of<0>([&](auto index) { return true; }) == true);
    BOOST_CHECK(bg::detail::all_indexes_of<0>([&](auto index) { return false; }) == true);
    BOOST_CHECK(bg::detail::any_index_of<0>([&](auto index) { return true; }) == false);
    BOOST_CHECK(bg::detail::any_index_of<0>([&](auto index) { return false; }) == false);
    BOOST_CHECK(bg::detail::none_index_of<0>([&](auto index) { return true; }) == true);
    BOOST_CHECK(bg::detail::none_index_of<0>([&](auto index) { return false; }) == true);
}

template <std::size_t I>
void test_dimension(bg::util::index_constant<I>)
{
    using point = bg::model::point<double, I, bg::cs::cartesian>;
    point p;
    bg::assign_value(p, 10.0);

    bg::detail::for_each_index<I>([&](auto index)
    {
        BOOST_CHECK(bg::get<index>(p) == 10.0);
        bg::set<index>(p, double(index));
    });
    bg::detail::for_each_dimension<point>([&](auto index)
    {
        BOOST_CHECK(bg::get<index>(p) == double(index));
    });

    BOOST_CHECK(
        bg::detail::all_indexes_of<0>([&](auto index)
        {
            return bg::get<index>(p) == double(index);
        }) == true);
    BOOST_CHECK(
        bg::detail::all_dimensions_of<point>([&](auto index)
        {
            return bg::get<index>(p) == 10;
        }) == false);
    BOOST_CHECK(
        bg::detail::any_index_of<0>([&](auto index)
        {
            return false;
        }) == false);
    BOOST_CHECK(
        bg::detail::any_dimension_of<point>([&](auto index)
        {
            return bg::get<index>(p) == double(I - 1);
        }) == true);
    BOOST_CHECK(
        bg::detail::none_index_of<0>([&](auto index)
        {
            return false;
        }) == true);
    BOOST_CHECK(
        bg::detail::none_dimension_of<point>([&](auto index)
        {
            return bg::get<index>(p) == double(0);
        }) == false);
}

template <std::size_t I, std::size_t N>
struct test_dimensions
{
    static void apply()
    {
        test_dimension(bg::util::index_constant<I>());
        test_dimensions<I + 1, N>::apply();
    }
};

template <std::size_t N>
struct test_dimensions<N, N>
{
    static void apply() {}
};

int test_main(int, char* [])
{
    test_dimensions<0, 5>::apply();

    return 0;
}
