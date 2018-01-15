/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#ifdef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
int main(){ return 0; }
#else

//[ args
#include <type_traits>
#include <memory>
#include <boost/callable_traits.hpp>

namespace ct = boost::callable_traits;

template<typename T, typename Expect>
void test(){
    using args_t = ct::args_t<T>;
    static_assert(std::is_same<args_t, Expect>::value, "");
}

int main() {

    {
        auto lamda = [](int, float&, const char*){};
        using lam = decltype(lamda);
        using expect = std::tuple<int, float&, const char*>;

        test<lam, expect>();
    }

    {
        struct foo;
        using pmf = void(foo::*)(int, float&, const char*);
        using expect = std::tuple<foo&, int, float&, const char*>;

        test<pmf, expect>();
    }

    {
        using function_ptr = void(*)(int, float&, const char*);
        using expect = std::tuple<int, float&, const char*>;
        test<function_ptr, expect>();
    }

    {
        using function_ref = void(&)(int, float&, const char*);
        using expect = std::tuple<int, float&, const char*>;
        test<function_ref, expect>();
    }

    {
        using function = void(int, float&, const char*);
        using expect = std::tuple<int, float&, const char*>;
        test<function, expect>();
    }

    {
        using abominable = void(int, float&, const char*) const;
        using expect = std::tuple<int, float&, const char*>;
        test<abominable, expect>();
    }
}
//]
#endif //#ifdef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
