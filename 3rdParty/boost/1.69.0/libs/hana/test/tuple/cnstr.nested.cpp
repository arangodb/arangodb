// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>

#include <type_traits>
namespace hana = boost::hana;


// See this bug: https://llvm.org/bugs/show_bug.cgi?id=24173

template <typename ...Xs>
constexpr hana::tuple<std::decay_t<Xs>...> f(Xs&& ...xs)
{ return hana::tuple<std::decay_t<Xs>...>{static_cast<Xs&&>(xs)...}; }

int main() {
    f(f(f(f(f(f(f(f(f(f(f(f(1))))))))))));
}
