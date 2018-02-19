// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/pair.hpp>

#include <type_traits>
namespace hana = boost::hana;


// Make sure the storage of a pair is compressed
struct empty { };
static_assert(sizeof(hana::pair<empty, int>) == sizeof(int), "");
static_assert(sizeof(hana::pair<int, empty>) == sizeof(int), "");

// Also make sure that a pair with only empty members is empty too. This is
// important to ensure, for example, that a tuple of pairs of empty objects
// will get the EBO.
struct empty1 { };
struct empty2 { };
static_assert(std::is_empty<hana::pair<empty1, empty2>>{}, "");

int main() { }
