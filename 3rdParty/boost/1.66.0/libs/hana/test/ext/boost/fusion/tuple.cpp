// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/fusion/tuple.hpp>

#include <boost/fusion/tuple.hpp>

#define MAKE_TUPLE(...) ::boost::fusion::make_tuple(__VA_ARGS__)
#define TUPLE_TYPE(...) ::boost::fusion::tuple<__VA_ARGS__>
#define TUPLE_TAG ::boost::hana::ext::boost::fusion::tuple_tag

#include "tests.hpp"
