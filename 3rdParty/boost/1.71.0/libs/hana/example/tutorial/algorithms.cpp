// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>

#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
namespace hana = boost::hana;
using namespace hana::literals;
using namespace std::literals;


int main() {

{

//! [reverse_transform]
auto to_str = [](auto const& x) {
  std::stringstream ss;
  ss << x;
  return ss.str();
};

auto xs = hana::make_tuple(1, 2.2, 'a', "bcde");

BOOST_HANA_RUNTIME_CHECK(
  hana::reverse(hana::transform(xs, to_str)) == hana::make_tuple("bcde", "a", "2.2", "1")
);
//! [reverse_transform]

//! [reverse_transform_copy]
hana::reverse(
  hana::transform(xs, to_str) // <-- copy into reverse(...) here?
);
//! [reverse_transform_copy]

//! [reverse_transform_move]
hana::reverse(
  hana::transform(xs, to_str) // <-- nope, move from the temporary instead!
);
//! [reverse_transform_move]

}{

//! [effects]
auto r = hana::any_of(hana::make_tuple("hello"s, 1.2, 3), [](auto x) {
  return std::is_integral<decltype(x)>{};
});

BOOST_HANA_CONSTANT_CHECK(r);
//! [effects]

{

//! [effects.codegen]
auto xs = hana::make_tuple("hello"s, 1.2, 3);
auto pred = [](auto x) { return std::is_integral<decltype(x)>{}; };

auto r = hana::bool_c<
  decltype(pred(xs[0_c]))::value ? true :
  decltype(pred(xs[1_c]))::value ? true :
  decltype(pred(xs[2_c]))::value ? true :
  false
>;

BOOST_HANA_CONSTANT_CHECK(r);
//! [effects.codegen]

}

}{

//! [cross_phase.setup]
struct Fish { std::string name; };
struct Cat  { std::string name; };
struct Dog  { std::string name; };

auto animals = hana::make_tuple(Fish{"Nemo"}, Cat{"Garfield"}, Dog{"Snoopy"});
//   ^^^^^^^ not a compile-time value

BOOST_HANA_CONSTANT_CHECK(hana::length(animals) == hana::size_c<3>);
//                        ^^^^^^^^^^^^^^^^^^^^^ assertion done at compile-time
//! [cross_phase.setup]

//! [cross_phase.is_empty]
BOOST_HANA_CONSTANT_CHECK(!hana::is_empty(animals));
//                         ^^^^^^^^^^^^^^^^^^^^^^^ assertion done at compile-time
//! [cross_phase.is_empty]

{

//! [cross_phase.any_of_runtime]
bool any_garfield = hana::any_of(animals, [](auto animal) {
  return animal.name == "Garfield"s;
});

BOOST_HANA_RUNTIME_CHECK(any_garfield);
//! [cross_phase.any_of_runtime]

}{

//! [cross_phase.any_of_compile_time]
auto any_cat = hana::any_of(animals, [](auto x) {
  return std::is_same<decltype(x), Cat>{};
});

BOOST_HANA_CONSTANT_CHECK(any_cat);
//! [cross_phase.any_of_compile_time]

}{

//! [cross_phase.any_of_explicit]
hana::integral_constant<bool, true> any_cat = hana::any_of(animals, [](auto x) {
  return std::is_same<decltype(x), Cat>{};
});

BOOST_HANA_CONSTANT_CHECK(any_cat);
//! [cross_phase.any_of_explicit]

}{

//! [cross_phase.filter]
auto mammals = hana::filter(animals, [](auto animal) {
  return hana::type_c<decltype(animal)> != hana::type_c<Fish>;
});
//! [cross_phase.filter]

BOOST_HANA_RUNTIME_CHECK(
  hana::transform(mammals, [](auto x) { return x.name; })
    == hana::make_tuple("Garfield", "Snoopy")
);

}

}{

//! [cross_phase.std::tuple_size]
std::tuple<int, char, std::string> xs{1, '2', std::string{"345"}};
static_assert(std::tuple_size<decltype(xs)>::value == 3u, "");
//! [cross_phase.std::tuple_size]

}

}
