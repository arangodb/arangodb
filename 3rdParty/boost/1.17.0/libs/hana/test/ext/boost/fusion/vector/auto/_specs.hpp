// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_EXT_BOOST_FUSION_VECTOR_AUTO_SPECS_HPP
#define BOOST_HANA_TEST_EXT_BOOST_FUSION_VECTOR_AUTO_SPECS_HPP

#define FUSION_MAX_VECTOR_SIZE 50
#include <boost/hana/ext/boost/fusion/vector.hpp>

#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/container/vector.hpp>

#define MAKE_TUPLE(...) ::boost::fusion::make_vector(__VA_ARGS__)
#define TUPLE_TYPE(...) ::boost::fusion::vector<__VA_ARGS__>
#define TUPLE_TAG ::boost::hana::ext::boost::fusion::vector_tag
#define MAKE_TUPLE_NO_CONSTEXPR

#endif // !BOOST_HANA_TEST_EXT_BOOST_FUSION_VECTOR_AUTO_SPECS_HPP
