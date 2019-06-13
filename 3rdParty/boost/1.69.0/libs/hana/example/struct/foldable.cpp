// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/second.hpp>
namespace hana = boost::hana;


struct Classroom {
    BOOST_HANA_DEFINE_STRUCT(Classroom,
        (unsigned short, boys),
        (unsigned short, girls)
    );
};

int main() {
    constexpr Classroom compsci{20, 3};

    BOOST_HANA_CONSTEXPR_CHECK(
        hana::fold_left(compsci, 0, [](auto total, auto member) {
            // first(member) is the name of the member, here
            // "boys" or "girls", and second(member) is its value.
            return hana::second(member) + total;
        }) == 23
    );
}
