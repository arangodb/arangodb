// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/printable.hpp>
#include <boost/hana/tuple.hpp>

#include <sstream>
#include <string>
namespace hana = boost::hana;


int main() {
    {
        std::ostringstream ss;
        ss << hana::experimental::print(hana::make_tuple());
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "()");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(hana::make_tuple(1));
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "(1)");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(hana::make_tuple(1, 2));
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "(1, 2)");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(hana::make_tuple(1, '2', "3456"));
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "(1, 2, 3456)");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(hana::make_tuple(1, '2', hana::make_tuple()));
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "(1, 2, ())");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(hana::make_tuple(1, '2', hana::make_tuple(3.3, '4')));
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "(1, 2, (3.3, 4))");
    }
}
