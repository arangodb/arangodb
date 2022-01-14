// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/arg.hpp>

#include <cstddef>
#include <utility>
namespace hana = boost::hana;


constexpr int to_int(char c)
{ return static_cast<int>(c) - 48; }

template <std::size_t N>
constexpr long long parse(const char (&arr)[N]) {
    long long number = 0, base = 1;
    for (std::size_t i = 0; i < N; ++i) {
        number += to_int(arr[N - 1 - i]) * base;
        base *= 10;
    }
    return number;
}

template <char ...c>
struct pick {
    static constexpr unsigned long n = parse<sizeof...(c)>({c...});

    template <typename ...T>
    constexpr auto operator()(T&& ...args) const
    { return hana::arg<n>(std::forward<T>(args)...); }
};

template <char ...c> constexpr pick<c...> operator"" _st() { return {}; }
template <char ...c> constexpr pick<c...> operator"" _nd() { return {}; }
template <char ...c> constexpr pick<c...> operator"" _rd() { return {}; }
template <char ...c> constexpr pick<c...> operator"" _th() { return {}; }


static_assert(1_st(1, '2', 3.3, 444) == 1, "");
static_assert(2_nd(1, '2', 3.3, 444) == '2', "");
static_assert(3_rd(1, '2', 3.3, 444) == 3.3, "");
static_assert(4_th(1, '2', 3.3, 444) == 444, "");

int main() { }
