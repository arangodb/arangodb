// Copyright Jason Rice 2020
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/define_struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/members.hpp>
#include <boost/hana/not_equal.hpp>
namespace hana = boost::hana;


struct SomeStruct {
    BOOST_HANA_DEFINE_STRUCT(SomeStruct, (int, x));

    constexpr bool operator==(SomeStruct const& other) {
        return hana::equal(hana::members(*this), hana::members(other));
    }

    constexpr bool operator!=(SomeStruct const& other) {
        return hana::not_equal(hana::members(*this), hana::members(other));
    }
};

int main() {
    static_assert(SomeStruct{5} == SomeStruct{5}, "");
    static_assert(hana::equal(SomeStruct{5}, SomeStruct{5}), "");
}
