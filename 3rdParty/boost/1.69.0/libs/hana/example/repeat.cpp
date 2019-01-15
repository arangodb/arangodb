// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/repeat.hpp>

#include <string>
namespace hana = boost::hana;


int main() {
    std::string s;
    for (char letter = 'a'; letter <= 'g'; ++letter)
        hana::repeat(hana::int_c<3>, [&] { s += letter; });

    BOOST_HANA_RUNTIME_CHECK(s == "aaabbbcccdddeeefffggg");
}
