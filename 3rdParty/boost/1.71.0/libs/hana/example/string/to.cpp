// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


constexpr auto str = hana::string_c<'h', 'i'>;

// using c_str()
constexpr char const* s1 = str.c_str();
static_assert(s1[0] == 'h' && s1[1] == 'i' && s1[2] == '\0', "");

// using hana::to
constexpr char const* s2 = hana::to<char const*>(str);
static_assert(s2[0] == 'h' && s2[1] == 'i' && s2[2] == '\0', "");

int main() { }
