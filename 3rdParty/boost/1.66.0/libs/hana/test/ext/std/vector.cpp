// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/vector.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
// #include <laws/functor.hpp>
#include <laws/orderable.hpp>

#include <vector>
namespace hana = boost::hana;


int main() {
    auto eqs = hana::make_tuple(
          std::vector<hana::test::eq<0>>{0}
        , std::vector<hana::test::eq<0>>{1}
        , std::vector<hana::test::eq<0>>{2}
        , std::vector<hana::test::eq<0>>{3}
        , std::vector<hana::test::eq<0>>{4}
    );

    // auto eq_values = hana::make_tuple(hana::test::eq<0>{}, hana::test::eq<2>{});

    auto ords = hana::make_tuple(
          std::vector<hana::test::ord<0>>{0}
        , std::vector<hana::test::ord<0>>{1}
        , std::vector<hana::test::ord<0>>{2}
        , std::vector<hana::test::ord<0>>{3}
        , std::vector<hana::test::ord<0>>{4}
    );

    //////////////////////////////////////////////////////////////////////////
    // Comparable, Orderable, Functor
    //////////////////////////////////////////////////////////////////////////
    hana::test::TestComparable<hana::ext::std::vector_tag>{eqs};
    hana::test::TestOrderable<hana::ext::std::vector_tag>{ords};
    // hana::test::TestFunctor<hana::ext::std::vector_tag>{eqs, eq_values};
}
