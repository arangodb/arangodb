// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/integral_constant.hpp>

#include <boost/hana/core/tag_of.hpp>

#include <type_traits>
namespace hana = boost::hana;


struct inherit_simple : std::integral_constant<int, 3> { };
struct inherit_no_default : std::integral_constant<int, 3> {
    inherit_no_default() = delete;
};

struct incomplete;
struct empty_type { };
struct non_pod { virtual ~non_pod() { } };


static_assert(std::is_same<
    hana::tag_of_t<inherit_simple>,
    hana::ext::std::integral_constant_tag<int>
>{}, "");
static_assert(std::is_same<
    hana::tag_of_t<inherit_no_default>,
    hana::ext::std::integral_constant_tag<int>
>{}, "");
static_assert(std::is_same<
    hana::tag_of_t<std::is_pointer<int*>>,
    hana::ext::std::integral_constant_tag<bool>
>{}, "");

static_assert(!std::is_same<
    hana::tag_of_t<incomplete>,
    hana::ext::std::integral_constant_tag<int>
>{}, "");
static_assert(!std::is_same<
    hana::tag_of_t<empty_type>,
    hana::ext::std::integral_constant_tag<int>
>{}, "");
static_assert(!std::is_same<
    hana::tag_of_t<non_pod>,
    hana::ext::std::integral_constant_tag<int>
>{}, "");
static_assert(!std::is_same<
    hana::tag_of_t<void>,
    hana::ext::std::integral_constant_tag<int>
>{}, "");

int main() { }
