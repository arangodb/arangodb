// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/printable.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>

#include <sstream>
#include <string>
namespace hana = boost::hana;


int main() {
    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_map()
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "{}");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_map(hana::make_pair(hana::int_c<1>, 'x'))
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "{1 => x}");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_map(hana::make_pair(hana::int_c<1>, 'x'),
                           hana::make_pair(hana::int_c<2>, 'y'))
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "{1 => x, 2 => y}");
    }

    {
        std::ostringstream ss;
        ss << hana::experimental::print(
            hana::make_map(hana::make_pair(hana::int_c<1>, 'x'),
                           hana::make_pair(hana::int_c<2>, 'y'),
                           hana::make_pair(hana::int_c<3>, 'z'))
        );
        BOOST_HANA_RUNTIME_CHECK(ss.str() == "{1 => x, 2 => y, 3 => z}");
    }
}
