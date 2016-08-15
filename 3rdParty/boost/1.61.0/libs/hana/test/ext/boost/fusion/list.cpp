// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/fusion/list.hpp>

#include <boost/fusion/container/generation/make_list.hpp>
#include <boost/fusion/container/list.hpp>

#define MAKE_TUPLE(...) ::boost::fusion::make_list(__VA_ARGS__)
#define TUPLE_TYPE(...) ::boost::fusion::list<__VA_ARGS__>
#define TUPLE_TAG ::boost::hana::ext::boost::fusion::list_tag

#include "tests.hpp"
