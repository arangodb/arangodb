// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/bool.hpp>
#include <boost/hana/tuple.hpp>

#include <support/seq.hpp>

#include <laws/base.hpp>
#include <laws/searchable.hpp>
using namespace boost::hana;


using test::ct_eq;

int main() {
    //////////////////////////////////////////////////////////////////////////
    // Laws with a minimal Searchable
    //////////////////////////////////////////////////////////////////////////
    {
        auto eqs = make<tuple_tag>(
              ::seq()
            , ::seq(ct_eq<0>{})
            , ::seq(ct_eq<0>{}, ct_eq<1>{})
            , ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
            , ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        );

        auto eq_keys = make<tuple_tag>(ct_eq<0>{}, ct_eq<3>{}, ct_eq<10>{});

        test::TestSearchable<::Seq>{eqs, eq_keys};

        auto bools = make<tuple_tag>(
              ::seq(true_c)
            , ::seq(false_c)
            , ::seq(true_c, true_c)
            , ::seq(true_c, false_c)
            , ::seq(false_c, true_c)
            , ::seq(false_c, false_c)
        );
        test::TestSearchable<::Seq>{bools, make<tuple_tag>(true_c, false_c)};
    }
}
