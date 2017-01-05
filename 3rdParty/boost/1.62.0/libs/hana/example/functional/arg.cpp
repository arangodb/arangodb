// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/arg.hpp>
namespace hana = boost::hana;


// hana::arg<0>(1, '2', 3.3); // static assertion (regardless of the number of arguments)
static_assert(hana::arg<1>(1, '2', 3.3) == 1, "");
static_assert(hana::arg<2>(1, '2', 3.3) == '2', "");
static_assert(hana::arg<3>(1, '2', 3.3) == 3.3, "");
// hana::arg<4>(1, '2', 3.3); // static assertion

int main() { }
