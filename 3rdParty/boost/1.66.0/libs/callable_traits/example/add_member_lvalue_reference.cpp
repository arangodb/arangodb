/*<-
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
int main(){ return 0; }
#else

//[ add_member_lvalue_reference
#include <type_traits>
#include <boost/callable_traits/add_member_lvalue_reference.hpp>

namespace ct = boost::callable_traits;

struct foo {};

int main() {

    {
        using pmf = void(foo::*)();
        using expect = void(foo::*)() &;
        using test = ct::add_member_lvalue_reference_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        // add_member_lvalue_reference_t doesn't change anything when
        // the function type already has an lvalue qualifier.
        using pmf = void(foo::*)() &;
        using expect = void(foo::*)() &;
        using test = ct::add_member_lvalue_reference_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        // add_member_lvalue_reference_t models C++11 reference collapsing
        // rules, so that adding an lvalue qualifier to an
        // rvalue-qualified type will force the lvalue.
        using pmf = void(foo::*)() &&;
        using expect = void(foo::*)() &;
        using test = ct::add_member_lvalue_reference_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        // add_member_lvalue_reference_t can also be used to create "abominable"
        // function types.
        using f = void();
        using expect = void() &;
        using test = ct::add_member_lvalue_reference_t<f>;
        static_assert(std::is_same<test, expect>::value, "");
    }
}

//]
#endif //#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
