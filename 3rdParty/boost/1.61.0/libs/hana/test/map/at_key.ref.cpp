// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>

#include <utility>
namespace hana = boost::hana;


template <typename T>
T const& cref(T& t) { return t; }

int main() {
    // using at_key
    {
        auto xs = hana::make_map(
            hana::make_pair(hana::int_c<0>, 0),
            hana::make_pair(hana::int_c<1>, '1'),
            hana::make_pair(hana::int_c<2>, 2.2)
        );

        // Make sure we return lvalue-references
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(xs, hana::int_c<0>) == 0);
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(xs, hana::int_c<1>) == '1');
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(xs, hana::int_c<2>) == 2.2);

        int& a = hana::at_key(xs, hana::int_c<0>);
        char& b = hana::at_key(xs, hana::int_c<1>);
        double& c = hana::at_key(xs, hana::int_c<2>);
        a = 9;
        b = '9';
        c = 9.9;

        // Make sure we return lvalue-references to const on a const map
        int const& ca = hana::at_key(cref(xs), hana::int_c<0>);
        char const& cb = hana::at_key(cref(xs), hana::int_c<1>);
        double const& cc = hana::at_key(cref(xs), hana::int_c<2>);

        BOOST_HANA_RUNTIME_CHECK(ca == 9);
        BOOST_HANA_RUNTIME_CHECK(cb == '9');
        BOOST_HANA_RUNTIME_CHECK(cc == 9.9);
    }

    // using operator[]
    {
        auto xs = hana::make_map(
            hana::make_pair(hana::int_c<0>, 0),
            hana::make_pair(hana::int_c<1>, '1'),
            hana::make_pair(hana::int_c<2>, 2.2)
        );

        BOOST_HANA_RUNTIME_CHECK(xs[hana::int_c<0>] == 0);
        BOOST_HANA_RUNTIME_CHECK(xs[hana::int_c<1>] == '1');
        BOOST_HANA_RUNTIME_CHECK(xs[hana::int_c<2>] == 2.2);

        xs[hana::int_c<0>] = 9;
        xs[hana::int_c<1>] = '9';
        xs[hana::int_c<2>] = 9.9;

        BOOST_HANA_RUNTIME_CHECK(xs[hana::int_c<0>] == 9);
        BOOST_HANA_RUNTIME_CHECK(xs[hana::int_c<1>] == '9');
        BOOST_HANA_RUNTIME_CHECK(xs[hana::int_c<2>] == 9.9);
    }

    // Make sure we return a rvalue-reference from a temporary map
    // (https://github.com/boostorg/hana/issues/90)
    {
        auto xs = hana::make_map(
            hana::make_pair(hana::int_c<0>, 0),
            hana::make_pair(hana::int_c<1>, '1'),
            hana::make_pair(hana::int_c<2>, 2.2)
        );

        char&& c = hana::at_key(std::move(xs), hana::int_c<1>);
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(xs, hana::int_c<1>) == '1');
        c = '9';
        BOOST_HANA_RUNTIME_CHECK(hana::at_key(xs, hana::int_c<1>) == '9');
    }
}
