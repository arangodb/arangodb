/*<-
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
int main(){ return 0; }
#else

//[ add_member_const
#include <type_traits>
#include <boost/callable_traits/add_member_const.hpp>

namespace ct = boost::callable_traits;

struct foo {};

int main() {

    {
        using pmf = int(foo::*)();
        using expect = int(foo::*)() const;
        using test = ct::add_member_const_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        // add_member_const_t doesn't change anything when
        // the function type is already const.
        using pmf = int(foo::*)() const &&;
        using expect = int(foo::*)() const &&;
        using test = ct::add_member_const_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        using pmf = int(foo::*)() volatile &;
        using expect = int(foo::*)() const volatile &;
        using test = ct::add_member_const_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        // add_member_const_t can also be used with "abominable"
        // function types.
        using f = int();
        using expect = int() const;
        using test = ct::add_member_const_t<f>;
        static_assert(std::is_same<test, expect>::value, "");
    }
}
//]
#endif //#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
