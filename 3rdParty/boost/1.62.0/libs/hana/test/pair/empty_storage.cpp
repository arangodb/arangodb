// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/pair.hpp>
namespace hana = boost::hana;


// Make sure the storage of a pair is compressed
struct empty { };
static_assert(sizeof(hana::pair<empty, int>) == sizeof(int), "");
static_assert(sizeof(hana::pair<int, empty>) == sizeof(int), "");

int main() { }
