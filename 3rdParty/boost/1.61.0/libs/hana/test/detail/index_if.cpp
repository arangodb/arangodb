// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/index_if.hpp>

#include <boost/hana/bool.hpp>
namespace hana = boost::hana;


template <int i>
struct x { };

template <int n>
struct Find {
    template <int i>
    constexpr auto operator()(x<i> const&) const
    { return hana::bool_c<n == i>; }
};

struct NotFound {
    template <typename T>
    constexpr auto operator()(T const&) const
    { return hana::false_c; }
};

struct Undefined;

int main() {
    {
        using Pack = hana::detail::pack<>;
        static_assert(hana::detail::index_if<Undefined, Pack>::value == 0, "");
    }

    {
        using Pack = hana::detail::pack<x<0>>;
        static_assert(hana::detail::index_if<Find<0>, Pack>::value == 0, "");
        static_assert(hana::detail::index_if<NotFound, Pack>::value == 1, "");
    }

    {
        using Pack = hana::detail::pack<x<0>, x<1>>;
        static_assert(hana::detail::index_if<Find<0>, Pack>::value == 0, "");
        static_assert(hana::detail::index_if<Find<1>, Pack>::value == 1, "");
        static_assert(hana::detail::index_if<NotFound, Pack>::value == 2, "");
    }

    {
        using Pack = hana::detail::pack<x<0>, x<1>, x<2>>;
        static_assert(hana::detail::index_if<Find<0>, Pack>::value == 0, "");
        static_assert(hana::detail::index_if<Find<1>, Pack>::value == 1, "");
        static_assert(hana::detail::index_if<Find<2>, Pack>::value == 2, "");
        static_assert(hana::detail::index_if<NotFound, Pack>::value == 3, "");
    }

    {
        using Pack = hana::detail::pack<x<0>, x<1>, x<2>, x<3>>;
        static_assert(hana::detail::index_if<Find<0>, Pack>::value == 0, "");
        static_assert(hana::detail::index_if<Find<1>, Pack>::value == 1, "");
        static_assert(hana::detail::index_if<Find<2>, Pack>::value == 2, "");
        static_assert(hana::detail::index_if<Find<3>, Pack>::value == 3, "");
        static_assert(hana::detail::index_if<NotFound, Pack>::value == 4, "");
    }

    {
        using Pack = hana::detail::pack<x<0>, x<1>, x<2>, x<3>, x<4>>;
        static_assert(hana::detail::index_if<Find<0>, Pack>::value == 0, "");
        static_assert(hana::detail::index_if<Find<1>, Pack>::value == 1, "");
        static_assert(hana::detail::index_if<Find<2>, Pack>::value == 2, "");
        static_assert(hana::detail::index_if<Find<3>, Pack>::value == 3, "");
        static_assert(hana::detail::index_if<Find<4>, Pack>::value == 4, "");
        static_assert(hana::detail::index_if<NotFound, Pack>::value == 5, "");
    }
}
