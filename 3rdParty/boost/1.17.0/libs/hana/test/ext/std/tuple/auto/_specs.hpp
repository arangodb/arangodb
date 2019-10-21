// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_EXT_STD_TUPLE_AUTO_SPECS_HPP
#define BOOST_HANA_TEST_EXT_STD_TUPLE_AUTO_SPECS_HPP

#include <boost/hana/ext/std/tuple.hpp>

#include <tuple>


#define MAKE_TUPLE(...) ::std::make_tuple(__VA_ARGS__)
#define TUPLE_TYPE(...) ::std::tuple<__VA_ARGS__>
#define TUPLE_TAG ::boost::hana::ext::std::tuple_tag

#endif // !BOOST_HANA_TEST_EXT_STD_TUPLE_AUTO_SPECS_HPP
