// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


struct X { };
static_assert(hana::sizeof_(hana::type_c<X>) == sizeof(X), "");

static_assert(hana::sizeof_(1) == sizeof(1), "");
static_assert(hana::sizeof_(hana::type_c<int>) == sizeof(int), "");

int main() {}
