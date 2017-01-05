// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/type_foldr1.hpp>

#include <type_traits>
using namespace boost::hana;


template <typename x, typename state>
struct f {
    struct type;
};

template <int>
struct x;

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>>::type,
    x<0>
>{}, "");

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>, x<1>>::type,
    f<x<0>, x<1>>::type
>{}, "");

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>, x<1>, x<2>>::type,
    f<x<0>, f<x<1>, x<2>>::type>::type
>{}, "");

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>, x<1>, x<2>, x<3>>::type,
    f<x<0>, f<x<1>, f<x<2>, x<3>>::type>::type>::type
>{}, "");

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>, x<1>, x<2>, x<3>, x<4>>::type,
    f<x<0>, f<x<1>, f<x<2>, f<x<3>, x<4>>::type>::type>::type>::type
>{}, "");

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>>::type,
    f<x<0>, f<x<1>, f<x<2>, f<x<3>, f<x<4>, x<5>>::type>::type>::type>::type>::type
>{}, "");

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>>::type,
    f<x<0>, f<x<1>, f<x<2>, f<x<3>, f<x<4>, f<x<5>, x<6>>::type>::type>::type>::type>::type>::type
>{}, "");

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>, x<7>>::type,
    f<x<0>, f<x<1>, f<x<2>, f<x<3>, f<x<4>, f<x<5>, f<x<6>, x<7>>::type>::type>::type>::type>::type>::type>::type
>{}, "");

static_assert(std::is_same<
    detail::type_foldr1<f, x<0>, x<1>, x<2>, x<3>, x<4>, x<5>, x<6>, x<7>, x<8>>::type,
    f<x<0>, f<x<1>, f<x<2>, f<x<3>, f<x<4>, f<x<5>, f<x<6>, f<x<7>, x<8>>::type>::type>::type>::type>::type>::type>::type>::type
>{}, "");

int main() { }
