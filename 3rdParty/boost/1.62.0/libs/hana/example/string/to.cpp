// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


constexpr auto str = hana::string_c<'h', 'i'>;
constexpr char const* s = hana::to<char const*>(str);
static_assert(s[0] == 'h' && s[1] == 'i' && s[2] == '\0', "");

int main() { }
