/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#ifdef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
int main(){ return 0; }
#else

//[ function_type
#include <type_traits>
#include <boost/callable_traits.hpp>

namespace ct = boost::callable_traits;

template<typename T>
void test(){

    // this example shows how boost::callable_traits::function_type_t
    // bevaves consistently for many different types
    using type = ct::function_type_t<T>;
    using expect = void(int, float&, const char*);
    static_assert(std::is_same<expect, type>{}, "");
}

int main() {

    auto lamda = [](int, float&, const char*){};
    using lam = decltype(lamda);
    test<lam>();

    using function_ptr = void(*)(int, float&, const char*);
    test<function_ptr>();

    using function_ref = void(&)(int, float&, const char*);
    test<function_ref>();

    using function = void(int, float&, const char*);
    test<function>();

    using abominable = void(int, float&, const char*) const;
    test<abominable>();
}
//]
#endif //#ifdef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
