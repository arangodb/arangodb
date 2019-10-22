// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/wrong.hpp>
namespace hana = boost::hana;


template <typename T, typename U>
struct base_template {
    // Can't write this because the assertion would always fire up:
    // static_assert(false, "...");

    // So instead we write this:
    static_assert(hana::detail::wrong<base_template<T, U>>::value,
    "base_template does not have a valid default definition");
};

template <>
struct base_template<int, int> {
    // something useful
};

int main() { }
