// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/type.hpp>

#include <string>
namespace hana = boost::hana;


int main() {
    {
        auto t0 = hana::make_map();
        auto t_implicit = t0;
        auto t_explicit(t0);

        (void)t_explicit;
        (void)t_implicit;
    }
    {
        auto t0 = hana::make_map(hana::make_pair(hana::int_c<2>, hana::int_c<20>));
        auto t_implicit = t0;
        auto t_explicit(t0);

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_map(hana::make_pair(hana::int_c<2>, hana::int_c<20>),
                                 hana::make_pair(hana::int_c<3>, hana::int_c<30>));
        auto t_implicit = t0;
        auto t_explicit(t0);

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_map(hana::make_pair(hana::int_c<2>, hana::int_c<20>),
                                 hana::make_pair(hana::int_c<3>, hana::int_c<30>),
                                 hana::make_pair(hana::type_c<void>, hana::type_c<void*>));
        auto t_implicit = t0;
        auto t_explicit(t0);

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        constexpr auto t0 = hana::make_map(
            hana::make_pair(hana::int_c<2>, hana::int_c<20>),
            hana::make_pair(hana::int_c<3>, hana::int_c<30>),
            hana::make_pair(hana::type_c<void>, hana::type_c<void*>));
        constexpr auto t_implicit = t0;
        constexpr auto t_explicit(t0);

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_map(hana::make_pair(hana::int_c<2>, std::string{"abcdef"}));
        auto copy = t0;
        BOOST_HANA_RUNTIME_CHECK(
            copy == hana::make_map(hana::make_pair(hana::int_c<2>, std::string{"abcdef"}))
        );
    }
}
