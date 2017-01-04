// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

template <typename ...xs>
struct list { };

template <typename T>
struct basic_type { using type = T; };

template <typename T>
constexpr basic_type<T> type{};


template <typename x, typename ...xs>
constexpr auto head(list<x, xs...>)
{ return type<x>; }

template <typename x, typename ...xs>
constexpr auto tail(list<x, xs...>)
{ return list<xs...>{}; }

template <typename F, typename State, typename X, typename ...Xs>
constexpr auto foldl(F f, State s, list<X, Xs...> xs)
{ return foldl(f, f(s, head(xs)), tail(xs)); }

template <typename F, typename State>
constexpr auto foldl(F, State s, list<>)
{ return s; }

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
