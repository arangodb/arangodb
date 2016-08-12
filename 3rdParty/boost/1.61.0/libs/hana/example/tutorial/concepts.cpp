// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/default.hpp>
#include <boost/hana/core/tag_of.hpp>

#include <string>
#include <type_traits>
namespace hana = boost::hana;


namespace with_special_base_class {
//! [special_base_class]
struct special_base_class { };

template <typename T>
struct print_impl : special_base_class {
  template <typename ...Args>
  static constexpr auto apply(Args&& ...) = delete;
};

template <typename T>
struct Printable {
  using Tag = hana::tag_of_t<T>;
  static constexpr bool value = !std::is_base_of<special_base_class, print_impl<Tag>>::value;
};
//! [special_base_class]

//! [special_base_class_customize]
struct Person { std::string name; };

template <>
struct print_impl<Person> /* don't inherit from special_base_class */ {
  // ... implementation ...
};

static_assert(Printable<Person>::value, "");
static_assert(!Printable<void>::value, "");
//! [special_base_class_customize]
}

namespace actual {
//! [actual]
template <typename T>
struct print_impl : hana::default_ {
  template <typename ...Args>
  static constexpr auto apply(Args&& ...) = delete;
};

template <typename T>
struct Printable {
  using Tag = hana::tag_of_t<T>;
  static constexpr bool value = !hana::is_default<print_impl<Tag>>::value;
};
//! [actual]

static_assert(!Printable<void>::value, "");
}

int main() { }
