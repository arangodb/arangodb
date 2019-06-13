// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/tuple.hpp>

#include <sstream>
namespace hana = boost::hana;


int main() {
    std::stringstream ss;
    hana::for_each(hana::make_tuple(0, '1', "234", 5.5), [&](auto x) {
        ss << x << ' ';
    });

    BOOST_HANA_RUNTIME_CHECK(ss.str() == "0 1 234 5.5 ");
}
