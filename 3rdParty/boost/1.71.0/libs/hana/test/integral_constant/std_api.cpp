// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>

#include <cstddef>
#include <type_traits>
namespace hana = boost::hana;


// operator()
static_assert(hana::size_c<0>() == 0, "");
static_assert(hana::size_c<1>() == 1, "");
static_assert(hana::int_c<-3>() == -3, "");

// decltype(operator())
static_assert(std::is_same<decltype(hana::size_c<0>()), std::size_t>{}, "");
static_assert(std::is_same<decltype(hana::int_c<-3>()), int>{}, "");

// conversions
constexpr std::size_t a = hana::size_c<0>, b = hana::size_c<1>;
static_assert(a == 0 && b == 1, "");

constexpr int c = hana::int_c<0>, d = hana::int_c<-3>;
static_assert(c == 0 && d == -3, "");

// nested ::value
static_assert(decltype(hana::int_c<1>)::value == 1, "");

// nested ::type
static_assert(std::is_same<
    decltype(hana::int_c<1>)::type,
    std::remove_cv_t<decltype(hana::int_c<1>)>
>{}, "");

// nested ::value_type
static_assert(std::is_same<decltype(hana::int_c<1>)::value_type, int>{}, "");

// inherits from std::integral_constant
static_assert(std::is_base_of<std::integral_constant<int, 3>,
                              hana::integral_constant<int, 3>>{}, "");

int main() { }
