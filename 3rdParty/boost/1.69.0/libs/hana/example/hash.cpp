// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
#include <utility>
namespace hana = boost::hana;


// Sample implementation of a compile-time set data structure. Of course,
// this naive implementation only works when no two elements of the same
// set have the same hash.
template <typename T>
struct bucket { };

template <typename ...T>
struct set
    : bucket<typename decltype(hana::hash(std::declval<T>()))::type>...
{ };

template <typename Set, typename T>
struct contains
    : std::is_base_of<
        bucket<typename decltype(hana::hash(std::declval<T>()))::type>,
        Set
    >
{ };

using Set = set<hana::int_<1>, hana::ulong<2>, hana::type<char>>;

static_assert(contains<Set, hana::int_<1>>{}, "");
static_assert(contains<Set, hana::ulong<2>>{}, "");
static_assert(contains<Set, hana::type<char>>{}, "");

static_assert(!contains<Set, hana::int_<3>>{}, "");
static_assert(!contains<Set, hana::type<float>>{}, "");

int main() { }
