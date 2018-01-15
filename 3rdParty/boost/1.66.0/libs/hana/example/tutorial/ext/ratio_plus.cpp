// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

//! [main]
#include <boost/hana/ext/std/ratio.hpp>
#include <boost/hana/plus.hpp>

#include <ratio>
namespace hana = boost::hana;

auto r = hana::plus(std::ratio<3, 4>{}, std::ratio<4, 5>{});
//! [main]

int main() { }
