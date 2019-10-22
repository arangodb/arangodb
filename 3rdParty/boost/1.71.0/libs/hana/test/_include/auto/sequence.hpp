// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_SEQUENCE_HPP
#define BOOST_HANA_TEST_AUTO_SEQUENCE_HPP

#include <boost/hana/concept/sequence.hpp>


static_assert(boost::hana::Sequence<TUPLE_TAG>::value, "");

#endif // !BOOST_HANA_TEST_AUTO_SEQUENCE_HPP
