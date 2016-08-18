// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;


struct Empty { };

int main() {
    {
        using T = hana::tuple<int>;
        const T t(3);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 3);
    }
    {
        using T = hana::tuple<std::string, int>;
        const T t("high", 5);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == "high");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == 5);
    }
    {
        using T = hana::tuple<double, int>;
        constexpr T t(2.718, 5);
        static_assert(hana::at_c<0>(t) == 2.718, "");
        static_assert(hana::at_c<1>(t) == 5, "");
    }
    {
        using T = hana::tuple<Empty>;
        constexpr T t{Empty()};
        constexpr Empty e = hana::at_c<0>(t); (void)e;
    }
    {
        using T = hana::tuple<double&, std::string, int>;
        double d = 1.5;
        const T t(d, "high", 5);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 1.5);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == "high");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == 5);
        hana::at_c<0>(t) = 2.5;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2.5);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == "high");
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == 5);
        BOOST_HANA_RUNTIME_CHECK(d == 2.5);
    }
}
