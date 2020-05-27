// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/type.hpp>

#include <string>
#include <type_traits>
namespace hana = boost::hana;


struct NoCopy {
    NoCopy() = default;
    NoCopy(NoCopy const&) = delete;
    friend auto operator==(NoCopy const&, NoCopy const&) { return hana::true_c; }
    friend auto operator!=(NoCopy const&, NoCopy const&) { return hana::false_c; }
};

// Note: It is also useful to check with a non-empty class, because that
//       triggers different instantiations due to EBO.
struct NoCopy_nonempty {
    NoCopy_nonempty() = default;
    NoCopy_nonempty(NoCopy_nonempty const&) = delete;
    int i;
    friend auto operator==(NoCopy_nonempty const&, NoCopy_nonempty const&) { return hana::true_c; }
    friend auto operator!=(NoCopy_nonempty const&, NoCopy_nonempty const&) { return hana::false_c; }
};

namespace boost { namespace hana {
    template <>
    struct hash_impl<NoCopy> {
        static constexpr auto apply(NoCopy const&)
        { return hana::type_c<NoCopy>; };
    };

    template <>
    struct hash_impl<NoCopy_nonempty> {
        static constexpr auto apply(NoCopy_nonempty const&)
        { return hana::type_c<NoCopy_nonempty>; };
    };
}}


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

    {
        using Map1 = hana::map<hana::pair<NoCopy, NoCopy>>;
        Map1 map1; (void)map1;
        static_assert(!std::is_copy_constructible<Map1>::value, "");

        using Map2 = hana::map<hana::pair<NoCopy_nonempty, NoCopy_nonempty>>;
        Map2 map2; (void)map2;
        static_assert(!std::is_copy_constructible<Map2>::value, "");
    }
}
