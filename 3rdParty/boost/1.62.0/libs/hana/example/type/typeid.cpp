// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/remove_if.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <string>
namespace hana = boost::hana;


struct Cat { std::string name; };
struct Dog { std::string name; };
struct Fish { std::string name; };

bool operator==(Cat const& a, Cat const& b) { return a.name == b.name; }
bool operator!=(Cat const& a, Cat const& b) { return a.name != b.name; }
bool operator==(Dog const& a, Dog const& b) { return a.name == b.name; }
bool operator!=(Dog const& a, Dog const& b) { return a.name != b.name; }
bool operator==(Fish const& a, Fish const& b) { return a.name == b.name; }
bool operator!=(Fish const& a, Fish const& b) { return a.name != b.name; }

int main() {
    hana::tuple<Cat, Fish, Dog, Fish> animals{
        Cat{"Garfield"}, Fish{"Jaws"}, Dog{"Beethoven"}, Fish{"Nemo"}
    };

    auto mammals = hana::remove_if(animals, [](auto const& a) {
        return hana::typeid_(a) == hana::type<Fish>{};
    });

    BOOST_HANA_RUNTIME_CHECK(mammals == hana::make_tuple(Cat{"Garfield"}, Dog{"Beethoven"}));
}
