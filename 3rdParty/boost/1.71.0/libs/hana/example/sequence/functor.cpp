// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>

#include <sstream>
#include <string>
namespace hana = boost::hana;


auto to_string = [](auto x) {
    std::ostringstream ss;
    ss << x;
    return ss.str();
};

int main() {
    BOOST_HANA_RUNTIME_CHECK(
        hana::transform(hana::make_tuple(1, '2', "345", std::string{"67"}), to_string) ==
        hana::make_tuple("1", "2", "345", "67")
    );
}
