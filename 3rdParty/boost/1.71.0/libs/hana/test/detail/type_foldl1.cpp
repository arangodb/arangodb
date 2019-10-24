// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/type_foldl1.hpp>

#include <type_traits>
namespace hana = boost::hana;


template <typename state, typename x>
struct f {
    struct type;
};

template <int>
struct x;

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>>::type,
    x<0>
>{}, "");

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>, x<1>>::type,
    f<x<0>, x<1>>::type
>{}, "");

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>, x<1>, x<2>>::type,
    f<f<x<0>, x<1>>::type, x<2>>::type
>{}, "");

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>, x<1>, x<2>, x<3>>::type,
    f<f<f<x<0>, x<1>>::type, x<2>>::type, x<3>>::type
>{}, "");

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>, x<1>, x<2>, x<3>, x<4>>::type,
    f<f<f<f<x<0>, x<1>>::type, x<2>>::type, x<3>>::type, x<4>>::type
>{}, "");

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>>::type,
    f<f<f<f<f<x<0>, x<1>>::type, x<2>>::type, x<3>>::type, x<4>>::type, x<5>>::type
>{}, "");

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>>::type,
    f<f<f<f<f<f<x<0>, x<1>>::type, x<2>>::type, x<3>>::type, x<4>>::type, x<5>>::type, x<6>>::type
>{}, "");

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>, x<7>>::type,
    f<f<f<f<f<f<f<x<0>, x<1>>::type, x<2>>::type, x<3>>::type, x<4>>::type, x<5>>::type, x<6>>::type, x<7>>::type
>{}, "");

static_assert(std::is_same<
    hana::detail::type_foldl1<f, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>, x<7>, x<8>>::type,
    f<f<f<f<f<f<f<f<x<0>, x<1>>::type, x<2>>::type, x<3>>::type, x<4>>::type, x<5>>::type, x<6>>::type, x<7>>::type, x<8>>::type
>{}, "");

int main() { }
