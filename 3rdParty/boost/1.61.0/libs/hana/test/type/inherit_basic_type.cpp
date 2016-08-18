// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


// `hana::type<T>` should inherit `hana::basic_type<T>`.

struct T;
static_assert(std::is_base_of<hana::basic_type<T>, decltype(hana::type_c<T>)>{}, "");
static_assert(std::is_base_of<hana::basic_type<T>, hana::type<T>>{}, "");

int main() { }
