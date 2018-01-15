// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/type_at.hpp>

#include <type_traits>
namespace hana = boost::hana;


template <int>
struct x;

static_assert(std::is_same<
    hana::detail::type_at<0, x<0>>::type,
    x<0>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<0, x<0>, x<1>>::type,
    x<0>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<0, x<0>, x<1>, x<2>>::type,
    x<0>
>{}, "");

static_assert(std::is_same<
    hana::detail::type_at<1, x<0>, x<1>>::type,
    x<1>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<1, x<0>, x<1>, x<2>>::type,
    x<1>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<1, x<0>, x<1>, x<2>, x<3>>::type,
    x<1>
>{}, "");

static_assert(std::is_same<
    hana::detail::type_at<2, x<0>, x<1>, x<2>>::type,
    x<2>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<2, x<0>, x<1>, x<2>, x<3>>::type,
    x<2>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<2, x<0>, x<1>, x<2>, x<3>, x<4>>::type,
    x<2>
>{}, "");

static_assert(std::is_same<
    hana::detail::type_at<3, x<0>, x<1>, x<2>, x<3>>::type,
    x<3>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<3, x<0>, x<1>, x<2>, x<3>, x<4>>::type,
    x<3>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<3, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>>::type,
    x<3>
>{}, "");

static_assert(std::is_same<
    hana::detail::type_at<4, x<0>, x<1>, x<2>, x<3>, x<4>>::type,
    x<4>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<4, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>>::type,
    x<4>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<4, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>>::type,
    x<4>
>{}, "");

static_assert(std::is_same<
    hana::detail::type_at<5, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>>::type,
    x<5>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<5, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>>::type,
    x<5>
>{}, "");
static_assert(std::is_same<
    hana::detail::type_at<5, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>, x<7>>::type,
    x<5>
>{}, "");

static_assert(std::is_same<
    hana::detail::type_at<6, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>>::type,
    x<6>
>{}, "");

static_assert(std::is_same<
    hana::detail::type_at<7, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>, x<7>>::type,
    x<7>
>{}, "");

int main() { }
