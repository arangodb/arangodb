// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/insert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;
using namespace hana::literals;
using namespace std::literals;


int main() {
    auto xs = hana::make_tuple("Hello"s, "world!"s);

    BOOST_HANA_RUNTIME_CHECK(
        hana::insert(xs, 1_c, " "s) == hana::make_tuple("Hello"s, " "s, "world!"s)
    );
}
