// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

// Make sure `assert` always triggers an assertion
#ifdef NDEBUG
#   undef NDEBUG
#endif

//! [additional_setup]
#include <cassert>
#include <iostream>
#include <string>

struct Fish { std::string name; };
struct Cat  { std::string name; };
struct Dog  { std::string name; };
//! [additional_setup]

//! [includes]
#include <boost/hana.hpp>
namespace hana = boost::hana;
//! [includes]


int main() {

//! [animals]
auto animals = hana::make_tuple(Fish{"Nemo"}, Cat{"Garfield"}, Dog{"Snoopy"});
//! [animals]

//! [algorithms]
using namespace hana::literals;

// Access tuple elements with operator[] instead of std::get.
Cat garfield = animals[1_c];

// Perform high level algorithms on tuples (this is like std::transform)
auto names = hana::transform(animals, [](auto a) {
  return a.name;
});

assert(hana::reverse(names) == hana::make_tuple("Snoopy", "Garfield", "Nemo"));
//! [algorithms]


//! [type-level]
auto animal_types = hana::make_tuple(hana::type_c<Fish*>, hana::type_c<Cat&>, hana::type_c<Dog>);

auto no_pointers = hana::remove_if(animal_types, [](auto a) {
  return hana::traits::is_pointer(a);
});

static_assert(no_pointers == hana::make_tuple(hana::type_c<Cat&>, hana::type_c<Dog>), "");
//! [type-level]


//! [has_name]
auto has_name = hana::is_valid([](auto&& x) -> decltype((void)x.name) { });

static_assert(has_name(garfield), "");
static_assert(!has_name(1), "");
//! [has_name]

#if 0
//! [screw_up]
auto serialize = [](std::ostream& os, auto const& object) {
  hana::for_each(os, [&](auto member) {
    //           ^^ oopsie daisy!
    os << member << std::endl;
  });
};
//! [screw_up]
#endif

//! [serialization]
// 1. Give introspection capabilities to 'Person'
struct Person {
  BOOST_HANA_DEFINE_STRUCT(Person,
    (std::string, name),
    (int, age)
  );
};

// 2. Write a generic serializer (bear with std::ostream for the example)
auto serialize = [](std::ostream& os, auto const& object) {
  hana::for_each(hana::members(object), [&](auto member) {
    os << member << std::endl;
  });
};

// 3. Use it
Person john{"John", 30};
serialize(std::cout, john);

// output:
// John
// 30
//! [serialization]

}
