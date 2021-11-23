// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


int main() {
    {
        auto t0 = hana::make_set();
        auto t_implicit = t0;
        auto t_explicit(t0);

        (void)t_explicit;
        (void)t_implicit;
    }
    {
        auto t0 = hana::make_set(hana::int_c<2>);
        auto t_implicit = t0;
        auto t_explicit(t0);

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_set(hana::int_c<2>, hana::int_c<3>);
        auto t_implicit = t0;
        auto t_explicit(t0);

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_set(hana::int_c<2>, hana::int_c<3>, hana::type_c<int>);
        auto t_implicit = t0;
        auto t_explicit(t0);

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        constexpr auto t0 = hana::make_set(hana::int_c<2>, hana::int_c<3>, hana::type_c<int>);
        constexpr auto t_implicit = t0;
        constexpr auto t_explicit(t0);

        (void)t_implicit;
        (void)t_explicit;
    }
}
