// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/back.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::back(hana::make_tuple(1, '2', 3.3)) == 3.3, "");
static_assert(hana::back(hana::make_tuple(1, '2', 3.3, nullptr)) == nullptr, "");

int main() { }
