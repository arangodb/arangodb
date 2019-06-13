// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/pair.hpp>

#include <type_traits>
namespace hana = boost::hana;


int main() {
    {
        struct T { }; struct U { };
        static_assert(std::is_same<
            hana::tag_of<hana::pair<T, U>>::type,
            hana::pair_tag
        >{}, "");
    }

    // Bug #1
    {
        using Pair = hana::pair<hana::integral_constant<int, 3>, int>;
        using PairTag = hana::tag_of<Pair>::type;
        using Tag = hana::tag_of<std::integral_constant<unsigned long, 0>>::type;
    }

    // Bug #2
    {
        using Pair = hana::pair<std::integral_constant<int, 3>, int>;
        using PairTag = hana::tag_of<Pair>::type;
        using Tag = hana::tag_of<std::integral_constant<unsigned long, 0>>::type;
    }
}
