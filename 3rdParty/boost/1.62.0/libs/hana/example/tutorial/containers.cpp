// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace hana = boost::hana;
using namespace hana::literals;
using namespace std::literals;


int main() {

{

//! [make<tuple_tag>]
auto xs = hana::make<hana::tuple_tag>(1, 2.2, 'a', "bcde"s);
//! [make<tuple_tag>]

}{

//! [make<range_tag>]
constexpr auto r = hana::make<hana::range_tag>(hana::int_c<3>, hana::int_c<10>);
static_assert(r == hana::make_range(hana::int_c<3>, hana::int_c<10>), "");
//! [make<range_tag>]

}{

//! [tuple_constructor]
hana::tuple<int, double, char, std::string> xs{1, 2.2, 'a', "bcde"s};
//! [tuple_constructor]
(void)xs;

}{

//! [types]
auto xs = hana::make_tuple(1, '2', "345");
auto ints = hana::make_range(hana::int_c<0>, hana::int_c<100>);
// what can we say about the types of `xs` and `ints`?
//! [types]
(void)xs;
(void)ints;

}{

//! [types_maximally_specified]
hana::tuple<int, char, char const*> xs = hana::make_tuple(1, '2', "345");
auto ints = hana::make_range(hana::int_c<0>, hana::int_c<100>);
// can't specify the type of ints, however
//! [types_maximally_specified]
(void)xs;
(void)ints;

}{

//! [lifetime]
std::string hello = "Hello";
std::vector<char> world = {'W', 'o', 'r', 'l', 'd'};

// hello is copied, world is moved-in
auto xs = hana::make_tuple(hello, std::move(world));

// s is a reference to the copy of hello inside xs.
// It becomes a dangling reference as soon as xs is destroyed.
std::string& s = xs[0_c];
//! [lifetime]
(void)s;

}{

//! [reference_wrapper]
std::vector<int> ints = { /* huge vector of ints */ };
std::vector<std::string> strings = { /* huge vector of strings */ };

auto map = hana::make_map(
  hana::make_pair(hana::type_c<int>, std::ref(ints)),
  hana::make_pair(hana::type_c<std::string>, std::ref(strings))
);

auto& v = map[hana::type_c<int>].get();
BOOST_HANA_RUNTIME_CHECK(&v == &ints);
//! [reference_wrapper]

}

}


namespace overloading {
//! [overloading]
template <typename T>
void f(std::vector<T> xs) {
  // ...
}

template <typename R, typename = std::enable_if_t<hana::is_a<hana::range_tag, R>()>>
void f(R r) {
  // ...
}
//! [overloading]
}
