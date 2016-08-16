// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/printable.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/string.hpp>

#include <sstream>
#include <string>
namespace hana = boost::hana;


int main() {
    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_set()
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "{}");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_set(hana::int_c<1>)
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "{1}");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_set(hana::int_c<1>, BOOST_HANA_STRING("3456"))
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "{1, \"3456\"}");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_set(hana::char_c<'x'>, BOOST_HANA_STRING("3456"))
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "{x, \"3456\"}");
    }
}
