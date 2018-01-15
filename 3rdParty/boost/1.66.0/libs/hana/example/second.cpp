// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/second.hpp>
#include <boost/hana/pair.hpp>
namespace hana = boost::hana;


static_assert(hana::second(hana::make_pair(1, 'x')) == 'x', "");

int main() { }
