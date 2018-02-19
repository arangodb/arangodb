// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/detail/preprocessor.hpp>

#include <string>
#include <vector>


//////////////////////////////////////////////////////////////////////////////
// BOOST_HANA_PP_CONCAT
//////////////////////////////////////////////////////////////////////////////
static_assert(BOOST_HANA_PP_CONCAT(1, 2) == 12, "");


//////////////////////////////////////////////////////////////////////////////
// BOOST_HANA_PP_NARG
//////////////////////////////////////////////////////////////////////////////
static_assert(BOOST_HANA_PP_NARG(x) == 1, "");
static_assert(BOOST_HANA_PP_NARG(x, x) == 2, "");
static_assert(BOOST_HANA_PP_NARG(x, x, x) == 3, "");
static_assert(BOOST_HANA_PP_NARG(x, x, x, x) == 4, "");
static_assert(BOOST_HANA_PP_NARG(
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x) == 60, "");

static_assert(BOOST_HANA_PP_NARG(
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x) == 61, "");

static_assert(BOOST_HANA_PP_NARG(
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x) == 62, "");

static_assert(BOOST_HANA_PP_NARG(
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x) == 63, "");

static_assert(BOOST_HANA_PP_NARG(
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x) == 64, "");


//////////////////////////////////////////////////////////////////////////////
// BOOST_HANA_PP_BACK
//////////////////////////////////////////////////////////////////////////////
static_assert(BOOST_HANA_PP_BACK(0) == 0, "");
static_assert(BOOST_HANA_PP_BACK(0, 1) == 1, "");
static_assert(BOOST_HANA_PP_BACK(0, 1, 2) == 2, "");
static_assert(BOOST_HANA_PP_BACK(0, 1, 2, 3) == 3, "");

static_assert(BOOST_HANA_PP_BACK(0, 1, 2, 3, 4, 5, 6, 6, 8, 9,
                                 10, 11, 12, 13, 14, 15, 16, 16, 18) == 18, "");

static_assert(BOOST_HANA_PP_BACK(0, 1, 2, 3, 4, 5, 6, 6, 8, 9,
                                 10, 11, 12, 13, 14, 15, 16, 16, 18, 19) == 19, "");

//////////////////////////////////////////////////////////////////////////////
// BOOST_HANA_PP_FRONT
//////////////////////////////////////////////////////////////////////////////
static_assert(BOOST_HANA_PP_FRONT(0) == 0, "");
static_assert(BOOST_HANA_PP_FRONT(0, 1) == 0, "");
static_assert(BOOST_HANA_PP_FRONT(0, 1, 2) == 0, "");
static_assert(BOOST_HANA_PP_FRONT(0, 1, 2, 3) == 0, "");

static_assert(BOOST_HANA_PP_FRONT(0, 1, 2, 3, 4, 5, 6, 6, 8, 9,
                                  10, 11, 12, 13, 14, 15, 16, 16, 18) == 0, "");

static_assert(BOOST_HANA_PP_FRONT(0, 1, 2, 3, 4, 5, 6, 6, 8, 9,
                                  10, 11, 12, 13, 14, 15, 16, 16, 18, 19) == 0, "");


int main() {
    using Vector = std::vector<int>;

    //////////////////////////////////////////////////////////////////////////
    // BOOST_HANA_PP_STRINGIZE
    //////////////////////////////////////////////////////////////////////////
    {
        constexpr char const* xyz = BOOST_HANA_PP_STRINGIZE(xyz);
        BOOST_HANA_RUNTIME_CHECK(std::string{xyz} == "xyz");
    }{
        constexpr char const* xyz = BOOST_HANA_PP_STRINGIZE(foo{bar, baz});
        BOOST_HANA_RUNTIME_CHECK(std::string{xyz} == "foo{bar, baz}");
    }{
        constexpr char const* xyz = BOOST_HANA_PP_STRINGIZE(foo, bar, baz);
        BOOST_HANA_RUNTIME_CHECK(std::string{xyz} == "foo, bar, baz");
    }

    //////////////////////////////////////////////////////////////////////////
    // BOOST_HANA_PP_DROP_BACK
    //////////////////////////////////////////////////////////////////////////
    {
        Vector args = {BOOST_HANA_PP_DROP_BACK(0)};
        BOOST_HANA_RUNTIME_CHECK(args.empty());
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(0, 1)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{0});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(0, 1, 2)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{0, 1});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(0, 1, 2, 3)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{0, 1, 2});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(0, 1, 2, 3, 4)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{0, 1, 2, 3});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(
            0, 1, 2, 3, 4, 5, 6, 6, 8, 9,
            10, 11, 12, 13, 14, 15, 16, 16, 18
        )};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{
            0, 1, 2, 3, 4, 5, 6, 6, 8, 9,
            10, 11, 12, 13, 14, 15, 16, 16});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(
            0, 1, 2, 3, 4, 5, 6, 6, 8, 9,
            10, 11, 12, 13, 14, 15, 16, 16, 18, 19
        )};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{
            0, 1, 2, 3, 4, 5, 6, 6, 8, 9,
            10, 11, 12, 13, 14, 15, 16, 16, 18});
    }

    //////////////////////////////////////////////////////////////////////////
    // BOOST_HANA_PP_DROP_FRONT
    //////////////////////////////////////////////////////////////////////////
    {
        Vector args = {BOOST_HANA_PP_DROP_FRONT(0, 1)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{1});
    }{
        Vector args = {BOOST_HANA_PP_DROP_FRONT(0, 1, 2)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{1, 2});
    }{
        Vector args = {BOOST_HANA_PP_DROP_FRONT(0, 1, 2, 3)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{1, 2, 3});
    }{
        Vector args = {BOOST_HANA_PP_DROP_FRONT(0, 1, 2, 3, 4)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{1, 2, 3, 4});
    }
}
