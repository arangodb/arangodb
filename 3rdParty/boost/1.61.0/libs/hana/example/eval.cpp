// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/eval.hpp>
#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/lazy.hpp>
namespace hana = boost::hana;


static_assert(hana::eval(hana::make_lazy(hana::_ + 1)(3)) == 4, "");

int main() { }
