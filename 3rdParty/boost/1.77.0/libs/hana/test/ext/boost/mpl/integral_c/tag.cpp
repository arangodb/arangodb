// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/mpl/integral_c.hpp>

#include <boost/hana/core/tag_of.hpp>

#include <boost/mpl/bool.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/long.hpp>
#include <boost/mpl/size_t.hpp>

#include <cstddef>
#include <type_traits>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


static_assert(std::is_same<
    hana::tag_of_t<mpl::bool_<true>>,
    hana::ext::boost::mpl::integral_c_tag<bool>
>::value, "");

static_assert(std::is_same<
    hana::tag_of_t<mpl::int_<0>>,
    hana::ext::boost::mpl::integral_c_tag<int>
>::value, "");

static_assert(std::is_same<
    hana::tag_of_t<mpl::long_<0>>,
    hana::ext::boost::mpl::integral_c_tag<long>
>::value, "");

static_assert(std::is_same<
    hana::tag_of_t<mpl::size_t<0>>,
    hana::ext::boost::mpl::integral_c_tag<std::size_t>
>::value, "");

static_assert(std::is_same<
    hana::tag_of_t<mpl::integral_c<int, 0>>,
    hana::ext::boost::mpl::integral_c_tag<int>
>::value, "");

static_assert(std::is_same<
    hana::tag_of_t<mpl::char_<0>>,
    hana::ext::boost::mpl::integral_c_tag<char>
>::value, "");

int main() { }
