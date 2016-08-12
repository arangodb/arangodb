// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/is_a.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


constexpr auto str = hana::string_c<'a', 'b', 'c', 'd', 'e', 'f'>;
BOOST_HANA_CONSTANT_CHECK(hana::is_a<hana::string_tag>(str));

int main() { }
