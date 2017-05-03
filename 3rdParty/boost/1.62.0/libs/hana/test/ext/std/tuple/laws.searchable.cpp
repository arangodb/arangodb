// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/searchable.hpp>

#include <tuple>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto tuples = hana::make_tuple(
          std::make_tuple()
        , std::make_tuple(ct_eq<0>{})
        , std::make_tuple(ct_eq<0>{}, ct_eq<1>{})
        , std::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    );
    auto keys = hana::make_tuple(ct_eq<3>{}, ct_eq<5>{}, ct_eq<7>{});

    auto bool_tuples = hana::make_tuple(
          std::make_tuple(hana::true_c)
        , std::make_tuple(hana::false_c)
        , std::make_tuple(hana::true_c, hana::true_c)
        , std::make_tuple(hana::true_c, hana::false_c)
        , std::make_tuple(hana::false_c, hana::true_c)
        , std::make_tuple(hana::false_c, hana::false_c)
    );
    auto bool_keys = hana::make_tuple(hana::true_c, hana::false_c);

    hana::test::TestSearchable<hana::ext::std::tuple_tag>{tuples, keys};
    hana::test::TestSearchable<hana::ext::std::tuple_tag>{bool_tuples, bool_keys};
}
