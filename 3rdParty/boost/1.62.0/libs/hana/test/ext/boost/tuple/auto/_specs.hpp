// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_EXT_BOOST_TUPLE_AUTO_SPECS_HPP
#define BOOST_HANA_TEST_EXT_BOOST_TUPLE_AUTO_SPECS_HPP

#include <boost/hana/ext/boost/tuple.hpp>

#include <boost/tuple/tuple.hpp>


#define MAKE_TUPLE(...) ::boost::make_tuple(__VA_ARGS__)
#define TUPLE_TYPE(...) ::boost::tuple<__VA_ARGS__>
#define TUPLE_TAG ::boost::hana::ext::boost::tuple_tag
#define MAKE_TUPLE_NO_CONSTEXPR

#endif // !BOOST_HANA_TEST_EXT_BOOST_TUPLE_AUTO_SPECS_HPP
