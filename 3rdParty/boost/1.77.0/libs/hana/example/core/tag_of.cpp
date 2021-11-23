// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/tag_of.hpp>

#include <type_traits>
namespace hana = boost::hana;


static_assert(std::is_same<hana::tag_of<int>::type, int>{}, "");
static_assert(std::is_same<hana::tag_of<int&>::type, int>{}, "");
static_assert(std::is_same<hana::tag_of<int const&>::type, int>{}, "");

struct PersonTag;
struct Person { using hana_tag = PersonTag; };
static_assert(std::is_same<hana::tag_of<Person>::type, PersonTag>{}, "");
static_assert(std::is_same<hana::tag_of<Person volatile&&>::type, PersonTag>{}, "");

int main() { }
