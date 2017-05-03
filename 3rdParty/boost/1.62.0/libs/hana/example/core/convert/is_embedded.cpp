// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/to.hpp>
namespace hana = boost::hana;


static_assert(hana::is_embedded<int, long>{}, "");

// int -> unsigned long could cause negative values to be lost
static_assert(!hana::is_embedded<int, unsigned int long>{}, "");

// similarly, float can't represent all the values of int
static_assert(!hana::is_embedded<int, float>{}, "");

// OK, the conversion is lossless
static_assert(hana::is_embedded<float, double>{}, "");

int main() { }
