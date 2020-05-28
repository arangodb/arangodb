/*<-

Copyright Barrett Adair 2016-2017

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
->*/

//[ remove_varargs
#include <type_traits>
#include <boost/callable_traits/remove_varargs.hpp>

namespace ct = boost::callable_traits;

struct foo {};

int main() {

    {
        using f = void(int, ...);
        using expect = void(int);
        using test = ct::remove_varargs_t<f>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        using fp = void(*)(...);
        using expect = void(*)();
        using test = ct::remove_varargs_t<fp>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        using fr = void(&)(const char*, ...);
        using expect = void(&)(const char*);
        using test = ct::remove_varargs_t<fr>;
        static_assert(std::is_same<test, expect>::value, "");
    } {
        using pmf = void(foo::*)(...) const;
        using expect = void(foo::*)() const;
        using test = ct::remove_varargs_t<pmf>;
        static_assert(std::is_same<test, expect>::value, "");
    }
}
//]
