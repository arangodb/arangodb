// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/core/is_a.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;
using namespace std::string_literals;


int main() {
    BOOST_HANA_RUNTIME_CHECK(
        hana::find_if(hana::make_tuple(1, '2', 3.3, "abc"s), hana::is_a<std::string>) == hana::just("abc"s)
    );

    BOOST_HANA_RUNTIME_CHECK(
        "abc"s ^hana::in^ hana::make_tuple(1, '2', 3.3, "abc"s)
    );
}
