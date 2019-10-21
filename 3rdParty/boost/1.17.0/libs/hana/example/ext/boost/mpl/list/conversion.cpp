// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/to.hpp>
#include <boost/hana/ext/boost/mpl/list.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <boost/mpl/list.hpp>
#include <type_traits>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


auto xs = hana::make_tuple(hana::type_c<int>, hana::type_c<char>, hana::type_c<double>);
static_assert(std::is_same<
    decltype(hana::to<hana::ext::boost::mpl::list_tag>(xs)),
    mpl::list<int, char, double>
>{}, "");

int main() { }
