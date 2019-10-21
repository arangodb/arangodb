/*<-
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#ifdef BOOST_CLBL_TRTS_DISABLE_VARIABLE_TEMPLATES
int main(){ return 0; }
#else

//[ intro
#include <type_traits>
#include <tuple>
#include <boost/callable_traits.hpp>

namespace ct = boost::callable_traits;

// This function template helps keep our example code neat
template<typename A, typename B>
void assert_same(){ static_assert(std::is_same<A, B>::value, ""); }

// foo is a function object
struct foo {
    void operator()(int, char, float) const {}
};

int main() {

    // Use args_t to retrieve a parameter list as a std::tuple:
    assert_same<
        ct::args_t<foo>,
        std::tuple<int, char, float>
    >();

    // has_void_return lets us perform a quick check for a void return type
    static_assert(ct::has_void_return<foo>::value, "");

    // Detect C-style variadics (ellipses) in a signature (e.g. printf)
    static_assert(!ct::has_varargs<foo>::value, "");

    // pmf is a pointer-to-member function: void (foo::*)(int, char, float) const
    using pmf = decltype(&foo::operator());

    // remove_member_const_t lets you remove the const member qualifier
    assert_same<
        ct::remove_member_const_t<pmf>,
        void (foo::*)(int, char, float) /*no const!*/
    >();

    // Conversely, add_member_const_t adds a const member qualifier
    assert_same<
        pmf,
        ct::add_member_const_t<void (foo::*)(int, char, float)>
    >();

    // is_const_member_v checks for the presence of member const
    static_assert(ct::is_const_member<pmf>::value, "");
}

//]
#endif
