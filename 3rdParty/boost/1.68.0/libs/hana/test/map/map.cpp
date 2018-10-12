// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/map.hpp>

#include <laws/base.hpp>
#include <support/minimal_product.hpp>
#include <type_traits>
#include <utility>
namespace hana = boost::hana;


template <typename ...Pairs>
struct check_map {
    static_assert(std::is_same<
        hana::map<Pairs...>,
        decltype(hana::make_map(std::declval<Pairs>()...))
    >{}, "");
};

template <int i>
using pair = ::product_t<hana::test::ct_eq<i>, hana::test::ct_eq<-i>>;

template struct check_map<>;
template struct check_map<pair<1>>;
template struct check_map<pair<1>, pair<2>>;
template struct check_map<pair<1>, pair<2>, pair<3>>;
template struct check_map<pair<1>, pair<2>, pair<3>, pair<4>>;
template struct check_map<pair<1>, pair<2>, pair<3>, pair<4>, pair<5>>;
template struct check_map<pair<1>, pair<2>, pair<3>, pair<4>, pair<5>, pair<6>>;

int main() { }
