// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <sstream>
#include <string>
#include <type_traits>
namespace hana = boost::hana;
using namespace std::literals;


auto to_string = [](auto x) {
    std::ostringstream ss;
    ss << x;
    return ss.str();
};

int main() {
    BOOST_HANA_RUNTIME_CHECK(
        hana::transform(hana::make_tuple(1, '2', "345", std::string{"67"}), to_string)
                ==
        hana::make_tuple("1", "2", "345", "67")
    );

    BOOST_HANA_CONSTANT_CHECK(hana::transform(hana::nothing, to_string) == hana::nothing);
    BOOST_HANA_RUNTIME_CHECK(hana::transform(hana::just(123), to_string) == hana::just("123"s));

    BOOST_HANA_CONSTANT_CHECK(
        hana::transform(hana::tuple_t<void, int(), char[10]>, hana::template_<std::add_pointer_t>)
                ==
        hana::tuple_t<void*, int(*)(), char(*)[10]>
    );
}
