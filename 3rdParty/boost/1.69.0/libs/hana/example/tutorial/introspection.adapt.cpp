// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <iostream>
#include <string>
namespace hana = boost::hana;
using namespace hana::literals;
using namespace std::literals;


//! [BOOST_HANA_DEFINE_STRUCT]
struct Person {
  BOOST_HANA_DEFINE_STRUCT(Person,
    (std::string, name),
    (int, age)
  );
};
//! [BOOST_HANA_DEFINE_STRUCT]

int main() {

//! [for_each]
Person john{"John", 30};

hana::for_each(john, [](auto pair) {
  std::cout << hana::to<char const*>(hana::first(pair)) << ": "
            << hana::second(pair) << std::endl;
});

// name: John
// age: 30
//! [for_each]

//! [for_each.fuse]
hana::for_each(john, hana::fuse([](auto name, auto member) {
  std::cout << hana::to<char const*>(name) << ": " << member << std::endl;
}));
//! [for_each.fuse]

#ifdef BOOST_HANA_CONFIG_ENABLE_STRING_UDL

{

//! [at_key]
std::string name = hana::at_key(john, "name"_s);
BOOST_HANA_RUNTIME_CHECK(name == "John");

int age = hana::at_key(john, "age"_s);
BOOST_HANA_RUNTIME_CHECK(age == 30);
//! [at_key]

}{

//! [to<map_tag>]
auto map = hana::insert(hana::to<hana::map_tag>(john), hana::make_pair("last name"_s, "Doe"s));

std::string name = map["name"_s];
BOOST_HANA_RUNTIME_CHECK(name == "John");

std::string last_name = map["last name"_s];
BOOST_HANA_RUNTIME_CHECK(last_name == "Doe");

int age = map["age"_s];
BOOST_HANA_RUNTIME_CHECK(age == 30);
//! [to<map_tag>]

}

#endif

}

//! [BOOST_HANA_ADAPT_STRUCT]
namespace not_my_namespace {
  struct Person {
    std::string name;
    int age;
  };
}

BOOST_HANA_ADAPT_STRUCT(not_my_namespace::Person, name, age);
//! [BOOST_HANA_ADAPT_STRUCT]


//! [BOOST_HANA_ADAPT_ADT]
namespace also_not_my_namespace {
  struct Person {
    std::string get_name();
    int get_age();
  };
}

BOOST_HANA_ADAPT_ADT(also_not_my_namespace::Person,
  (name, [](auto const& p) { return p.get_name(); }),
  (age, [](auto const& p) { return p.get_age(); })
);
//! [BOOST_HANA_ADAPT_ADT]
