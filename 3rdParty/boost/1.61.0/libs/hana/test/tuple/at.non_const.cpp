// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;


struct Empty {};

struct S {
    constexpr S()
        : a{1, Empty{}},
          k(hana::at_c<0>(a)),
          e(hana::at_c<1>(a))
    { }

    hana::tuple<int, Empty> a;
    int k;
    Empty e;
};

constexpr hana::tuple<int, int> getP () { return { 3, 4 }; }

int main() {
    {
        using T = hana::tuple<int>;
        T t(3);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 3);
        hana::at_c<0>(t) = 2;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
    }
    {
        using T = hana::tuple<std::string, int>;
        T t("high", 5);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == "high");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == 5);
        hana::at_c<0>(t) = "four";
        hana::at_c<1>(t) = 4;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == "four");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == 4);
    }
    {
        using T = hana::tuple<double&, std::string, int>;
        double d = 1.5;
        T t(d, "high", 5);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 1.5);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == "high");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == 5);
        hana::at_c<0>(t) = 2.5;
        hana::at_c<1>(t) = "four";
        hana::at_c<2>(t) = 4;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2.5);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == "four");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == 4);
        BOOST_HANA_RUNTIME_CHECK(d == 2.5);
    }
    // get on a rvalue tuple
    {
        static_assert(hana::at_c<0>(hana::tuple<float, int, double, long>{0.0f, 1, 2.0, 3L}) == 0, "" );
        static_assert(hana::at_c<1>(hana::tuple<float, int, double, long>{0.0f, 1, 2.0, 3L}) == 1, "" );
        static_assert(hana::at_c<2>(hana::tuple<float, int, double, long>{0.0f, 1, 2.0, 3L}) == 2, "" );
        static_assert(hana::at_c<3>(hana::tuple<float, int, double, long>{0.0f, 1, 2.0, 3L}) == 3, "" );
        static_assert(S().k == 1, "");
        static_assert(hana::at_c<1>(getP()) == 4, "");
    }
    {
        // make sure get<> returns the right types
        struct T { };
        struct U { };
        struct V { };

        hana::tuple<T, U, V> xs{};
        (void)static_cast<T>(hana::at_c<0>(xs));
        (void)static_cast<U>(hana::at_c<1>(xs));
        (void)static_cast<V>(hana::at_c<2>(xs));
    }
}
