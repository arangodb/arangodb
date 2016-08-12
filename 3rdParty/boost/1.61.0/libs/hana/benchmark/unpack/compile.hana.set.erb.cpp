// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

// Disable concept checks, otherwise x<> would need to be Comparable.
// Only define the macro if it was not defined on the command line.
#ifndef BOOST_HANA_CONFIG_DISABLE_CONCEPT_CHECKS
#   define BOOST_HANA_CONFIG_DISABLE_CONCEPT_CHECKS
#endif

#include <boost/hana/set.hpp>
#include <boost/hana/unpack.hpp>
namespace hana = boost::hana;


struct f {
    template <typename ...T>
    constexpr void operator()(T const& ...) const { }
};

template <int i>
struct x { };

int main() {
    constexpr auto set = hana::make_set(
        <%= (1..input_size).map { |n| "x<#{n}>{}" }.join(', ') %>
    );
    hana::unpack(set, f{});
}
