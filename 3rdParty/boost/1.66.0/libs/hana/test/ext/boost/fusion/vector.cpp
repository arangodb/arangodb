// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/fusion/vector.hpp>

#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/container/vector.hpp>

#define MAKE_TUPLE(...) ::boost::fusion::make_vector(__VA_ARGS__)
#define TUPLE_TYPE(...) ::boost::fusion::vector<__VA_ARGS__>
#define TUPLE_TAG ::boost::hana::ext::boost::fusion::vector_tag

#include "tests.hpp"
