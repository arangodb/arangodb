// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


// This test makes sure that tuples containing pairs behave properly. It is
// useful to test this specific case because tuple and pair (may) share a lot
// of implementation details (for EBO), and that can easily lead to subtle bugs.
// For example, if both pair and tuple inherit from similar base classes for
// EBO, accessing a tuple of pairs or a pair of tuples (which requires some
// base class casting magic) can lead to ambiguous overloads.

int main() {
    struct empty1 { };
    struct empty2 { };

    {
        hana::tuple<hana::pair<empty1, empty2>> t{};
        hana::first(hana::at_c<0>(t));
        hana::second(hana::at_c<0>(t));
    }
    {
        hana::tuple<hana::pair<int, empty2>> t{};
        hana::first(hana::at_c<0>(t));
        hana::second(hana::at_c<0>(t));
    }
    {
        hana::tuple<hana::pair<int, int>> t{};
        hana::first(hana::at_c<0>(t));
        hana::second(hana::at_c<0>(t));
    }
    {
        hana::tuple<hana::pair<empty1, int>> t{};
        hana::first(hana::at_c<0>(t));
        hana::second(hana::at_c<0>(t));
    }

    {
        hana::pair<hana::tuple<>, hana::tuple<>> p{};
        hana::first(p);
        hana::second(p);
    }
    {
        hana::pair<hana::tuple<int>, hana::tuple<int>> p{};
        hana::first(p);
        hana::second(p);
    }
    {
        hana::pair<hana::tuple<>, hana::tuple<int>> p{};
        hana::first(p);
        hana::second(p);
    }
    {
        hana::pair<hana::tuple<empty1>, hana::tuple<empty2>> p{};
        hana::first(p);
        hana::second(p);
    }
    {
        hana::pair<hana::tuple<empty1, empty2>, hana::tuple<empty2>> p{};
        hana::first(p);
        hana::second(p);
    }
    {
        hana::pair<hana::tuple<empty1, empty2>, hana::tuple<empty1, empty2>> p{};
        hana::first(p);
        hana::second(p);
    }
}
