// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/common.hpp>

#include <type_traits>
namespace hana = boost::hana;


template <typename T>
struct ImplicitConvertibleTo {
    constexpr operator T() const { return {}; }
};

struct T { };
struct invalid;

static_assert(std::is_same<hana::common_t<T, T>, T>{}, "");
static_assert(std::is_same<hana::common_t<invalid, invalid>, invalid>{}, "");
static_assert(std::is_same<hana::common_t<void, void>, void>{}, "");

static_assert(std::is_same<hana::common_t<ImplicitConvertibleTo<T>, T>, T>{}, "");
static_assert(std::is_same<hana::common_t<T, ImplicitConvertibleTo<T>>, T>{}, "");

static_assert(hana::has_common<T, T>{}, "");
static_assert(!hana::has_common<void, T>{}, "");
static_assert(!hana::has_common<T, void>{}, "");
static_assert(!hana::has_common<invalid, T>{}, "");
static_assert(!hana::has_common<T, invalid>{}, "");

int main() { }
