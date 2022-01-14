// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;


/*
When we use `int_<...>` in a template, Clang 3.5 says:

--------------------------------
include/boost/hana/integral_constant.hpp:80:20: error: constexpr variable 'int_<1>' must be initialized by a constant expression
    constexpr auto int_ = integral<int, i>;
                   ^      ~~~~~~~~~~~~~~~~
test/integral/constexpr_bug.cpp:41:37: note: in instantiation of variable template specialization 'boost::hana::int_' requested here
constexpr auto check_int() { return int_<1>; }
                                    ^
include/boost/hana/integral_constant.hpp:80:27: note: subexpression not valid in a constant expression
    constexpr auto int_ = integral<int, i>;
                          ^
include/boost/hana/integral_constant.hpp:80:27: note: in call to 'integral_type(integral)'
--------------------------------

if we define int_ & friends like

    template <int i>
    constexpr auto int_ = integral<int, i>;

Instead, we do

    template <int i>
    constexpr decltype(integral<int, i>) int_{};

which is equivalent but uglier. Note that everything works just fine when
we're not in a template.
*/

template <typename T>
constexpr auto check_int() { return hana::int_c<1>; }

template <typename T>
constexpr auto check_true() { return hana::true_c; }

template <typename T>
constexpr auto check_size_t() { return hana::size_c<0>; }


int main() { }
