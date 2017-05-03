// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/prefix.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;
using namespace std::literals;


int main() {
    BOOST_HANA_RUNTIME_CHECK(
        hana::prefix(hana::make_tuple("dog"s, "car"s, "house"s), "my"s)
            ==
        hana::make_tuple("my", "dog", "my", "car", "my", "house")
    );
}
