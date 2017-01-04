// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/decay.hpp>

#include <type_traits>
namespace hana = boost::hana;


template <typename T, typename Decayed>
void check() {
    static_assert(std::is_same<
        typename hana::detail::decay<T>::type,
        Decayed
    >::value, "");
}

int main() {
    // void is untouched
    check<void, void>();

    // normal types lose cv-qualifiers
    check<int, int>();
    check<int const, int>();
    check<int const volatile, int>();

    // [cv-qualified] references are stripped
    check<int&, int>();
    check<int const&, int>();
    check<int&&, int>();
    check<int const&&, int>();

    // pointers are untouched
    check<int*, int*>();
    check<int const*, int const*>();
    check<int volatile*, int volatile*>();
    check<int const volatile*, int const volatile*>();

    // arrays decay to pointers
    check<int[], int*>();
    check<int[10], int*>();
    check<int const[10], int const*>();
    check<int volatile[10], int volatile*>();
    check<int const volatile[10], int const volatile*>();

    // functions decay to function pointers
    check<void(), void(*)()>();
    check<void(...), void (*)(...)>();
    check<void(int), void(*)(int)>();
    check<void(int, ...), void(*)(int, ...)>();
    check<void(int, float), void(*)(int, float)>();
    check<void(int, float, ...), void(*)(int, float, ...)>();
}
