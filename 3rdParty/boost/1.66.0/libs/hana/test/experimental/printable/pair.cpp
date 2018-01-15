// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/printable.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/pair.hpp>

#include <sstream>
namespace hana = boost::hana;


int main() {
    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_pair(1, hana::int_c<1>)
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "(1, 1)");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_pair(1, hana::int_c<2>)
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "(1, 2)");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_pair('x', hana::int_c<2>)
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "(x, 2)");
    }
}
