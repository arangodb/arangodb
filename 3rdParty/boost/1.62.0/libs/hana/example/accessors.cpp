// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/accessors.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/second.hpp>
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
    constexpr auto accessors = hana::accessors<Person>();
    BOOST_HANA_CONSTANT_CHECK(
        hana::first(accessors[hana::size_c<0>]) == BOOST_HANA_STRING("name")
    );

    BOOST_HANA_CONSTANT_CHECK(
        hana::first(accessors[hana::size_c<1>]) == BOOST_HANA_STRING("age")
    );

    constexpr auto get_name = hana::second(accessors[hana::size_c<0>]);
    constexpr auto get_age = hana::second(accessors[hana::size_c<1>]);

    Person john{"John", 30};
    BOOST_HANA_RUNTIME_CHECK(get_name(john) == "John");
    BOOST_HANA_RUNTIME_CHECK(get_age(john) == 30);
}
