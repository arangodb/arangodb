// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/detail/struct_macros.hpp>

#include <string>
#include <vector>


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
    x, x, x, x, x, x, x, x) == 38, "");
static_assert(BOOST_HANA_PP_NARG(
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x) == 39, "");
static_assert(BOOST_HANA_PP_NARG(
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x,
    x, x, x, x, x, x, x, x, x, x) == 40, "");


//////////////////////////////////////////////////////////////////////////////
// BOOST_HANA_PP_BACK
//////////////////////////////////////////////////////////////////////////////
static_assert(BOOST_HANA_PP_BACK(1) == 1, "");
static_assert(BOOST_HANA_PP_BACK(1, 2) == 2, "");
static_assert(BOOST_HANA_PP_BACK(1, 2, 3) == 3, "");
static_assert(BOOST_HANA_PP_BACK(1, 2, 3, 4) == 4, "");

static_assert(BOOST_HANA_PP_BACK(1,   2,  3,  4,  5,  6,  7,  8,  9, 10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
                                 31, 32, 33, 34, 35, 36, 37, 38, 39) == 39, "");

static_assert(BOOST_HANA_PP_BACK(1,   2,  3,  4,  5,  6,  7,  8,  9, 10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
                                 31, 32, 33, 34, 35, 36, 37, 38) == 38, "");

static_assert(BOOST_HANA_PP_BACK(1,   2,  3,  4,  5,  6,  7,  8,  9, 10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
                                 31, 32, 33, 34, 35, 36, 37, 38, 39, 40) == 40, "");


int main() {
    using Vector = std::vector<int>;

    //////////////////////////////////////////////////////////////////////////
    // BOOST_HANA_PP_DROP_BACK
    //////////////////////////////////////////////////////////////////////////
    {
        Vector args = {BOOST_HANA_PP_DROP_BACK(1)};
        BOOST_HANA_RUNTIME_CHECK(args.empty());
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(1, 2)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{1});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(1, 2, 3)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{1, 2});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(1, 2, 3, 4)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{1, 2, 3});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(1, 2, 3, 4, 5)};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{1, 2, 3, 4});
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(
             1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
            21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
            31, 32, 33, 34, 35, 36, 37, 38, 39
        )};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{
             1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
            21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
            31, 32, 33, 34, 35, 36, 37, 38
        });
    }{
        Vector args = {BOOST_HANA_PP_DROP_BACK(
             1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
            21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
            31, 32, 33, 34, 35, 36, 37, 38, 39, 40
        )};
        BOOST_HANA_RUNTIME_CHECK(args == Vector{
             1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
            21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
            31, 32, 33, 34, 35, 36, 37, 38, 39
        });
    }
}
