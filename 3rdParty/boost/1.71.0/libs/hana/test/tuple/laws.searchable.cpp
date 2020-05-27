// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/searchable.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto eq_tuples = hana::make_tuple(
          hana::make_tuple()
        , hana::make_tuple(ct_eq<0>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
    );
    hana::test::TestSearchable<hana::tuple_tag>{
        eq_tuples,
        hana::make_tuple(ct_eq<3>{}, ct_eq<5>{})
    };

    auto bool_tuples = hana::make_tuple(
          hana::make_tuple(hana::true_c)
        , hana::make_tuple(hana::false_c)
        , hana::make_tuple(hana::true_c, hana::true_c)
        , hana::make_tuple(hana::true_c, hana::false_c)
        , hana::make_tuple(hana::false_c, hana::true_c)
        , hana::make_tuple(hana::false_c, hana::false_c)
    );
    hana::test::TestSearchable<hana::tuple_tag>{
        bool_tuples,
        hana::make_tuple(hana::true_c, hana::false_c)
    };
}
