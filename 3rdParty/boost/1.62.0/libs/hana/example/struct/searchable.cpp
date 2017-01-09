// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/string.hpp>

#include <string>
namespace hana = boost::hana;


struct Person {
    BOOST_HANA_DEFINE_STRUCT(Person,
        (std::string, name),
        (unsigned short, age)
    );
};

int main() {
    Person john{"John", 30};

    BOOST_HANA_RUNTIME_CHECK(
        hana::find(john, BOOST_HANA_STRING("name")) == hana::just("John")
    );

    BOOST_HANA_CONSTANT_CHECK(
        hana::find(john, BOOST_HANA_STRING("foobar")) == hana::nothing
    );
}
