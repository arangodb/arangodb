// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/string.hpp>

#include <string>
namespace hana = boost::hana;


struct Person {
    BOOST_HANA_DEFINE_STRUCT(Person,
        (std::string, name),
        (std::string, last_name),
        (int, age)
    );
};

int main() {
    // non-const ref
    {
        Person john{"John", "Doe", 30};
        std::string& name = hana::at_key(john, BOOST_HANA_STRING("name"));
        std::string& last_name = hana::at_key(john, BOOST_HANA_STRING("last_name"));
        int& age = hana::at_key(john, BOOST_HANA_STRING("age"));

        name = "Bob";
        last_name = "Foo";
        age = 99;

        BOOST_HANA_RUNTIME_CHECK(john.name == "Bob");
        BOOST_HANA_RUNTIME_CHECK(john.last_name == "Foo");
        BOOST_HANA_RUNTIME_CHECK(john.age == 99);
    }

    // const ref
    {
        Person john{"John", "Doe", 30};
        Person const& const_john = john;
        std::string const& name = hana::at_key(const_john, BOOST_HANA_STRING("name"));
        std::string const& last_name = hana::at_key(const_john, BOOST_HANA_STRING("last_name"));
        int const& age = hana::at_key(const_john, BOOST_HANA_STRING("age"));

        john.name = "Bob";
        john.last_name = "Foo";
        john.age = 99;

        BOOST_HANA_RUNTIME_CHECK(name == "Bob");
        BOOST_HANA_RUNTIME_CHECK(last_name == "Foo");
        BOOST_HANA_RUNTIME_CHECK(age == 99);
    }
}
