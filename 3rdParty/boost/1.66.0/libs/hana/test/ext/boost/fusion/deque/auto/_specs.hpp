// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_EXT_BOOST_FUSION_DEQUE_AUTO_SPECS_HPP
#define BOOST_HANA_TEST_EXT_BOOST_FUSION_DEQUE_AUTO_SPECS_HPP

#include <boost/hana/ext/boost/fusion/deque.hpp>

#include <boost/fusion/container/deque.hpp>
#include <boost/fusion/container/generation/make_deque.hpp>


#define MAKE_TUPLE(...) ::boost::fusion::make_deque(__VA_ARGS__)
#define TUPLE_TYPE(...) ::boost::fusion::deque<__VA_ARGS__>
#define TUPLE_TAG ::boost::hana::ext::boost::fusion::deque_tag

#endif // !BOOST_HANA_TEST_EXT_BOOST_FUSION_DEQUE_AUTO_SPECS_HPP
