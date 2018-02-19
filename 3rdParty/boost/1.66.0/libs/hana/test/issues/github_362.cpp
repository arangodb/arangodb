// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;
using namespace hana::literals;


static constexpr auto x = 4'321'000_c;
static_assert(decltype(x)::value == 4'321'000, "");

int main() { }
