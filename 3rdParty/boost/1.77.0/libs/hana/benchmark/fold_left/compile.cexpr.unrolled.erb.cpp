// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/variadic/foldl1.hpp>


template <typename ...xs>
struct list { };

template <typename T>
struct basic_type { using type = T; };

template <typename T>
constexpr basic_type<T> type{};


template <typename F, typename State, typename ...Xs>
constexpr auto foldl(F f, State s, list<Xs...> xs)
{ return boost::hana::detail::variadic::foldl(f, s, type<Xs>...); }

//////////////////////////////////////////////////////////////////////////////

struct f {
    template <typename ...>
    struct result { };

    template <typename X, typename Y>
    constexpr auto operator()(X, Y) const
    { return result<X, Y>{}; }
};

template <int> struct x { };
struct state { };

int main() {
    constexpr auto xs = list<
        <%= (1..input_size).map { |i| "x<#{i}>" }.join(', ') %>
    >{};

    constexpr auto result = foldl(f{}, state{}, xs);
    (void)result;
}
