// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;


int main() {
    {
        using T = hana::tuple<>;
        T t0;
        T t;
        t = t0;
    }
    {
        using T = hana::tuple<int>;
        T t0(2);
        T t;
        t = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
    }
    {
        using T = hana::tuple<int, char>;
        T t0(2, 'a');
        T t;
        t = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == 'a');
    }
    {
        using T = hana::tuple<int, char, std::string>;
        const T t0(2, 'a', "some text");
        T t;
        t = t0;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<0>(t) == 2);
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<1>(t) == 'a');
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(t) == "some text");
    }
}
