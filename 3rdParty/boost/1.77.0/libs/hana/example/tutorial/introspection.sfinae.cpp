// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <string>
#include <vector>
namespace hana = boost::hana;


struct yes { std::string toString() const { return "yes"; } };
struct no { };

//! [optionalToString.sfinae]
template <typename T>
std::string optionalToString(T const& obj) {
  auto maybe_toString = hana::sfinae([](auto&& x) -> decltype(x.toString()) {
    return x.toString();
  });

  return maybe_toString(obj).value_or("toString not defined");
}
//! [optionalToString.sfinae]

int main() {
BOOST_HANA_RUNTIME_CHECK(optionalToString(yes{}) == "yes");
BOOST_HANA_RUNTIME_CHECK(optionalToString(no{}) == "toString not defined");

{

//! [maybe_add]
auto maybe_add = hana::sfinae([](auto x, auto y) -> decltype(x + y) {
  return x + y;
});

maybe_add(1, 2); // hana::just(3)

std::vector<int> v;
maybe_add(v, "foobar"); // hana::nothing
//! [maybe_add]

}
}
