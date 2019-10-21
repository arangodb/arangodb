// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>

#include <type_traits>
namespace hana = boost::hana;


//
// This test checks that we can NOT construct a tuple holding array members,
// per the standard.
//

int main() {
    static_assert(!std::is_constructible<hana::tuple<int[3]>, int[3]>{}, "");
    static_assert(!std::is_constructible<hana::tuple<int[3], float[4]>, int[3], float[4]>{}, "");
}
