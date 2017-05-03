// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/integral_constant.hpp>

#include <type_traits>
namespace hana = boost::hana;


// Make sure we have the right tag, even when including ext/std/integral_constant.hpp
static_assert(std::is_same<
    hana::tag_of_t<hana::integral_constant<int, 10>>,
    hana::integral_constant_tag<int>
>{}, "");

struct derived : hana::integral_constant<int, 10> { };
static_assert(std::is_same<
    hana::tag_of_t<derived>,
    hana::integral_constant_tag<int>
>{}, "");


int main() { }
