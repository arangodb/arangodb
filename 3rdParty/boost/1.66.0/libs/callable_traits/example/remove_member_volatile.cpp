/*<-
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
int main(){ return 0; }
#else

//[ remove_member_volatile
#include <type_traits>
#include <boost/callable_traits/remove_member_volatile.hpp>

namespace ct = boost::callable_traits;

struct foo {};

int main() {

    {
        using pmf = int(foo::*)() const volatile;
        using expect = int(foo::*)() const;
        using test = ct::remove_member_volatile_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        using pmf = int(foo::*)() volatile &&;
        using expect = int(foo::*)() &&;
        using test = ct::remove_member_volatile_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        using pmf = int(foo::*)() volatile &;
        using expect = int(foo::*)() &;
        using test = ct::remove_member_volatile_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        using f = int() const volatile;
        using expect = int() const;
        using test = ct::remove_member_volatile_t<f>;
        static_assert(std::is_same<test, expect>::value, "");
    }
}
//]
#endif //#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
