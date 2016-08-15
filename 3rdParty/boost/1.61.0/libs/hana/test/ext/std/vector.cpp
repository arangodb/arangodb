// Copyright Louis Dionne 2013-2016
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
using namespace boost::hana;


int main() {
    auto eqs = make_tuple(
          std::vector<test::eq<0>>{0}
        , std::vector<test::eq<0>>{1}
        , std::vector<test::eq<0>>{2}
        , std::vector<test::eq<0>>{3}
        , std::vector<test::eq<0>>{4}
    );

    // auto eq_values = make<tuple_tag>(test::eq<0>{}, test::eq<2>{});

    auto ords = make_tuple(
          std::vector<test::ord<0>>{0}
        , std::vector<test::ord<0>>{1}
        , std::vector<test::ord<0>>{2}
        , std::vector<test::ord<0>>{3}
        , std::vector<test::ord<0>>{4}
    );

    //////////////////////////////////////////////////////////////////////////
    // Comparable, Orderable, Functor
    //////////////////////////////////////////////////////////////////////////
    test::TestComparable<ext::std::vector_tag>{eqs};
    test::TestOrderable<ext::std::vector_tag>{ords};
    // test::TestFunctor<ext::std::vector_tag>{eqs, eq_values};
}
