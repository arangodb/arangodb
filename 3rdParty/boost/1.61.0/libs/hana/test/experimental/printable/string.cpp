// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/printable.hpp>
#include <boost/hana/string.hpp>

#include <sstream>
namespace hana = boost::hana;


int main() {
    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            BOOST_HANA_STRING("")
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "\"\"");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            BOOST_HANA_STRING("x")
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "\"x\"");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            BOOST_HANA_STRING("xy")
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "\"xy\"");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            BOOST_HANA_STRING("xyz")
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "\"xyz\"");
    }
}
