// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/default.hpp>
namespace hana = boost::hana;


template <typename T>
struct method_impl : hana::default_ { };

template <>
struct method_impl<int> { };

static_assert(hana::is_default<method_impl<void>>{}, "");
static_assert(!hana::is_default<method_impl<int>>{}, "");

int main() { }
