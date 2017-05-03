// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>

#include <type_traits>
namespace hana = boost::hana;


int main() {
    {
        typedef std::pair<int, short> P1;
        hana::pair<int, short> p1(3, 4);
        hana::pair<int, short> p2 = p1;
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == 3);
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == 4);
    }

    static_assert(std::is_trivially_copy_constructible<hana::pair<int, int>>::value, "");

    // make sure it also works constexpr
    {
        constexpr hana::pair<int, short> p1(3, 4);
        constexpr hana::pair<int, short> p2 = p1;
        static_assert(hana::first(p2) == 3, "");
        static_assert(hana::second(p2) == 4, "");
    }

    // Make sure it works across pair types (pair<T, U> -> pair<U, V>)
    {
        hana::pair<int, short> p1(3, 4);
        hana::pair<double, long> p2 = p1;
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == 3);
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == 4);
    }

    // And also constexpr across pair types
    {
        constexpr hana::pair<int, short> p1(3, 4);
        constexpr hana::pair<double, long> p2 = p1;
        static_assert(hana::first(p2) == 3, "");
        static_assert(hana::second(p2) == 4, "");
    }
}
