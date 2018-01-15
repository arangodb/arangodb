// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/basic_tuple.hpp>

#include <laws/base.hpp>

#include <string>
namespace hana = boost::hana;


struct Empty { };

int main() {
    {
        using T = hana::basic_tuple<>;
        T t0;
        T t_implicit = t0;
        T t_explicit(t0);

        (void)t_explicit;
        (void)t_implicit;
    }
    {
        using T = hana::basic_tuple<int>;
        T t0(2);
        T t = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
    }
    {
        using T = hana::basic_tuple<int, char>;
        T t0(2, 'a');
        T t = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == 'a');
    }
    {
        using T = hana::basic_tuple<int, char, std::string>;
        const T t0(2, 'a', "some text");
        T t = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == 'a');
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == "some text");
    }
    {
        using T = hana::basic_tuple<int>;
        constexpr T t0(2);
        constexpr T t = t0;
        static_assert(hana::at_c<0>(t) == 2, "");
    }
    {
        using T = hana::basic_tuple<Empty>;
        constexpr T t0{};
        constexpr T t = t0;
        constexpr Empty e = hana::at_c<0>(t); (void)e;
    }
    {
        struct T { };
        struct U { };

        constexpr hana::basic_tuple<T, U> binary{};
        constexpr hana::basic_tuple<T, U> copy_implicit = binary;
        constexpr hana::basic_tuple<T, U> copy_explicit(binary);

        (void)copy_implicit;
        (void)copy_explicit;
    }

    // This used to fail
    {
        hana::basic_tuple<
            hana::test::ct_eq<0>,
            hana::test::ct_eq<2>,
            hana::test::ct_eq<4>
        > tuple{};

        hana::basic_tuple<
            hana::test::ct_eq<0>,
            hana::test::ct_eq<2>,
            hana::test::ct_eq<4>
        > copy(tuple);
    }
}
