// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/fast_and.hpp>
namespace hana = boost::hana;


static_assert(hana::detail::fast_and<>::value, "");
static_assert(hana::detail::fast_and<true>::value, "");
static_assert(!hana::detail::fast_and<false>::value, "");

static_assert(hana::detail::fast_and<true, true>::value, "");
static_assert(!hana::detail::fast_and<true, false>::value, "");
static_assert(!hana::detail::fast_and<false, true>::value, "");
static_assert(!hana::detail::fast_and<false, false>::value, "");

static_assert(hana::detail::fast_and<true, true, true>::value, "");
static_assert(!hana::detail::fast_and<true, true, false>::value, "");
static_assert(!hana::detail::fast_and<true, false, true>::value, "");
static_assert(!hana::detail::fast_and<true, false, false>::value, "");
static_assert(!hana::detail::fast_and<false, true, true>::value, "");
static_assert(!hana::detail::fast_and<false, true, false>::value, "");
static_assert(!hana::detail::fast_and<false, false, true>::value, "");
static_assert(!hana::detail::fast_and<false, false, false>::value, "");

static_assert(hana::detail::fast_and<true, true, true, true, true, true>::value, "");
static_assert(!hana::detail::fast_and<true, true, true, false, true, true>::value, "");
static_assert(!hana::detail::fast_and<true, true, true, true, true, false>::value, "");
static_assert(!hana::detail::fast_and<false, true, true, true, true, true>::value, "");

int main() { }
