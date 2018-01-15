// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept.hpp>
namespace hana = boost::hana;


using T = int;

BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Applicative<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Comonad<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Comparable<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Constant<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::EuclideanRing<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Foldable<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Functor<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Group<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Hashable<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::IntegralConstant<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Iterable<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Logical<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Metafunction<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Monad<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::MonadPlus<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Monoid<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Orderable<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Product<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Ring<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Searchable<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Sequence<T>>{});
BOOST_HANA_CONSTANT_CHECK(hana::IntegralConstant<hana::Struct<T>>{});

int main() { }
