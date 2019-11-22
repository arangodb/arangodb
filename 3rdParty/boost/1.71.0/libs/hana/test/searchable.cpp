// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/tuple.hpp>

#include <support/seq.hpp>

#include <laws/base.hpp>
#include <laws/searchable.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    //////////////////////////////////////////////////////////////////////////
    // Laws with a minimal Searchable
    //////////////////////////////////////////////////////////////////////////
    {
        auto eqs = hana::make_tuple(
              ::seq()
            , ::seq(ct_eq<0>{})
            , ::seq(ct_eq<0>{}, ct_eq<1>{})
            , ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
            , ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        );

        auto eq_keys = hana::make_tuple(ct_eq<0>{}, ct_eq<3>{}, ct_eq<10>{});

        hana::test::TestSearchable<::Seq>{eqs, eq_keys};

        auto bools = hana::make_tuple(
              ::seq(hana::true_c)
            , ::seq(hana::false_c)
            , ::seq(hana::true_c, hana::true_c)
            , ::seq(hana::true_c, hana::false_c)
            , ::seq(hana::false_c, hana::true_c)
            , ::seq(hana::false_c, hana::false_c)
        );
        hana::test::TestSearchable<::Seq>{bools, hana::make_tuple(hana::true_c, hana::false_c)};
    }
}
