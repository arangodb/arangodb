// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/fwd/equal.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/type.hpp>

#include "../../at_key/pair.hpp"
namespace hana = boost::hana;


template <int i>
struct x { };

namespace boost { namespace hana {
    template <int i, int j>
    struct equal_impl<x<i>, x<j>> {
        static constexpr hana::bool_<i == j> apply(x<i> const&, x<j> const&)
        { return {}; }
    };

    template <int i>
    struct hash_impl<x<i>> {
        static constexpr hana::type<x<i>> apply(x<i> const&) { return {}; }
    };
}}

struct undefined { };

int main() {
    auto map = hana::make_map(<%=
        (1..input_size).map { |n| "light_pair<x<#{n}>, undefined>{}" }.join(', ')
    %>);
    (void)map;
}
