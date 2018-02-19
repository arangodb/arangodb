// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/extract.hpp>
#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/lazy.hpp>
namespace hana = boost::hana;


static_assert(hana::extract(hana::make_lazy(1)) == 1, "");
static_assert(hana::extract(hana::make_lazy(hana::_ + 1)(3)) == 4, "");

int main() { }
