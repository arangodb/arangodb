// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/common.hpp>

#include <type_traits>
namespace hana = boost::hana;


struct Person { };
struct Employee : Person { };

static_assert(std::is_same<hana::common<int, float>::type, float>{}, "");
static_assert(std::is_same<hana::common<Person, Employee>::type, Person>{}, "");

int main() { }
