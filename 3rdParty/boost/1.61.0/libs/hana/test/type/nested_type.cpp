// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


// Makes sure that `hana::type`s have a nested ::type alias

struct T;

static_assert(std::is_same<decltype(hana::type_c<T>)::type, T>{}, "");
static_assert(std::is_same<hana::type<T>::type, T>{}, "");

int main() { }
