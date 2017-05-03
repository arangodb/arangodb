// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#define BOOST_HANA_TEST_FOLDABLE_FOLD_LEFT_MCD

#if BOOST_HANA_TEST_PART == 1
#   define BOOST_HANA_TEST_ITERABLE
#elif BOOST_HANA_TEST_PART == 2
#   define BOOST_HANA_TEST_MONAD
#elif BOOST_HANA_TEST_PART == 3
#   define BOOST_HANA_TEST_MONAD_PLUS
#elif BOOST_HANA_TEST_PART == 4
#   define BOOST_HANA_TEST_ORDERABLE
#elif BOOST_HANA_TEST_PART == 5
#   define BOOST_HANA_TEST_SEARCHABLE
#elif BOOST_HANA_TEST_PART == 6
#   define BOOST_HANA_TEST_SEQUENCE
#endif

#include <laws/templates/seq.hpp>
