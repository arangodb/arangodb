// Copyright 2013-2019 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX14_CONSTEXPR) && !defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && (!defined(_MSC_VER) || (_MSC_VER > 1916))

//[type_index_constexpr14_sort_check_example
/*`
    The following example shows that `boost::typeindex::ctti_type_index` is usable at compile time on
    a C++14 compatible compilers to check order of template parameters.

    Consider the situation when we have a function that accepts std::tuple, boost::variant or some other class that uses variadic templates:
*/

template <class... T> class types{};

template <class... T>
void do_something(const types<T...>& t) noexcept;

/*`
    Such functions may be very usefull, however they may significantly increase the size of the resulting program. Each instantionation of such function with different templates order
    consumes space in the resulting program:

        // Types are same, but different order leads to new instantionation of do_something function.
        types<bool, double, int>
        types<bool, int, double>
        types<int, bool, double>
        types<int, double, bool>
        types<double, int, bool>
        types<double, bool, int>

    One of the ways to reduce instantinations count is to force the types to have some order:
*/


#include <boost/type_index/ctti_type_index.hpp>

// Implementing type trait that returns true if the types are sorted lexographicaly
template <class... T>
constexpr bool is_asc_sorted(types<T...>) noexcept {
    return true;
}

template <class Lhs, class Rhs, class... TN>
constexpr bool is_asc_sorted(types<Lhs, Rhs, TN...>) noexcept {
    using namespace boost::typeindex;
    return ctti_type_index::type_id<Lhs>() <= ctti_type_index::type_id<Rhs>()
        && is_asc_sorted(types<Rhs, TN...>());
}


// Using the newly created `is_asc_sorted` trait:
template <class... T>
void do_something(const types<T...>& /*value*/) noexcept {
    static_assert(
        is_asc_sorted( types<T...>() ),
        "T... for do_something(const types<T...>& t) must be sorted ascending"
    );
}

int main() {
    do_something( types<bool, double, int>() );
    // do_something( types<bool, int, double>() ); // Fails the static_assert!
}
//] [/type_index_constexpr14_sort_check_example]

#else // #if !defined(BOOST_NO_CXX14_CONSTEXPR) && !defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && (!defined(_MSC_VER) || (_MSC_VER > 1916))

int main() {}

#endif

