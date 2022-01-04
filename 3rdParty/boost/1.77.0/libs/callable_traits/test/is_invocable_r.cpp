/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <utility>
#include <boost/callable_traits/is_invocable.hpp>
#include "test.hpp"

#ifdef BOOST_CLBL_TRTS_GCC_OLDER_THAN_4_9_2
//gcc < 4.9 doesn't like the invoke_case pattern used here
int main(){}
#else

template<typename T>
struct tag {
    using type = T;
};

template<bool Expect, typename Ret, typename... Args>
struct invoke_case {
   template<typename Callable>
   void operator()(tag<Callable>) const {

// when available, test parity with std implementation (c++2a breaks our expectations but we still match std impl)
#if defined(__cpp_lib_is_invocable) || __cplusplus >= 201707L
        CT_ASSERT((std::is_invocable_r<Ret, Callable, Args...>() == boost::callable_traits::is_invocable_r<Ret, Callable, Args...>()));
#  ifndef BOOST_CLBL_TRTS_DISABLE_VARIABLE_TEMPLATES
        CT_ASSERT((std::is_invocable_r_v<Ret, Callable, Args...> == boost::callable_traits::is_invocable_r_v<Ret, Callable, Args...>));
#  endif
#else
        CT_ASSERT((Expect == boost::callable_traits::is_invocable_r<Ret, Callable, Args...>()));
#  ifndef BOOST_CLBL_TRTS_DISABLE_VARIABLE_TEMPLATES
        CT_ASSERT((Expect == boost::callable_traits::is_invocable_r_v<Ret, Callable, Args...>));
#  endif
#endif

   }
};

template<typename Callable, typename... InvokeCases>
void run_tests() {
    using ignored = int[];
    ignored x {(InvokeCases{}(tag<Callable>{}),0)..., 0};
    (void)x;
}

struct foo {};

int main() {

    run_tests<void(foo::*)()
        ,invoke_case<true, void, foo>
        ,invoke_case<true, void, foo*>
        ,invoke_case<true, void, foo&>
        ,invoke_case<true, void, foo&&>
        ,invoke_case<true, void, std::reference_wrapper<foo>>
        ,invoke_case<false, int, foo>
        ,invoke_case<false, int, foo*>
        ,invoke_case<false, int, foo&>
        ,invoke_case<false, int, foo&&>
        ,invoke_case<false, int, std::reference_wrapper<foo>>
        ,invoke_case<false, void, foo const>
        ,invoke_case<false, void, foo const*>
        ,invoke_case<false, void, foo const&>
        ,invoke_case<false, void, foo const&&>
        ,invoke_case<false, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
        ,invoke_case<false, int, foo const>
        ,invoke_case<false, int, foo const*>
        ,invoke_case<false, int, foo const&>
        ,invoke_case<false, int, foo const&&>
        ,invoke_case<false, int, std::reference_wrapper<foo const>>
        ,invoke_case<false, int, foo, int>
        ,invoke_case<false, int, foo*, int>
        ,invoke_case<false, int, foo&, int>
        ,invoke_case<false, int, foo&&, int>
        ,invoke_case<false, int, std::reference_wrapper<foo>, int>
    >();

    run_tests<char(foo::*)()
        ,invoke_case<true, void, foo>
        ,invoke_case<true, void, foo*>
        ,invoke_case<true, void, foo&>
        ,invoke_case<true, void, foo&&>
        ,invoke_case<true, void, std::reference_wrapper<foo>>
        ,invoke_case<true, int, foo>
        ,invoke_case<true, int, foo*>
        ,invoke_case<true, int, foo&>
        ,invoke_case<true, int, foo&&>
        ,invoke_case<true, int, std::reference_wrapper<foo>>
    >();

    run_tests<void(foo::*)() LREF
        ,invoke_case<false, void, foo>
        ,invoke_case<true, void, foo*>
        ,invoke_case<true, void, foo&>
        ,invoke_case<false, int, foo*>
        ,invoke_case<false, int, foo&>
        ,invoke_case<false, void, foo&&>
        ,invoke_case<true, void, std::reference_wrapper<foo>>
        ,invoke_case<false, int, std::reference_wrapper<foo>>
        ,invoke_case<false, void, foo const>
        ,invoke_case<false, void, foo const*>
        ,invoke_case<false, void, foo const&>
        ,invoke_case<false, void, foo const&&>
        ,invoke_case<false, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();

    run_tests<void(foo::*)() RREF
        ,invoke_case<true, void, foo>
        ,invoke_case<false, int, foo>
        ,invoke_case<false, void, foo*>
        ,invoke_case<false, void, foo&>
        ,invoke_case<true, void, foo&&>
        ,invoke_case<false, int, foo&&>
        ,invoke_case<false, void, std::reference_wrapper<foo>>
        ,invoke_case<false, void, foo const>
        ,invoke_case<false, void, foo const*>
        ,invoke_case<false, void, foo const&>
        ,invoke_case<false, void, foo const&&>
        ,invoke_case<false, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();

    run_tests<void(foo::*)() const
        ,invoke_case<true, void, foo>
        ,invoke_case<true, void, foo*>
        ,invoke_case<true, void, foo&>
        ,invoke_case<true, void, foo&&>
        ,invoke_case<true, void, std::reference_wrapper<foo>>
        ,invoke_case<true, void, foo const>
        ,invoke_case<true, void, foo const*>
        ,invoke_case<true, void, foo const&>
        ,invoke_case<true, void, foo const&&>
        ,invoke_case<true, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, int, foo>
        ,invoke_case<false, int, foo*>
        ,invoke_case<false, int, foo&>
        ,invoke_case<false, int, foo&&>
        ,invoke_case<false, int, std::reference_wrapper<foo>>
        ,invoke_case<false, int, foo const>
        ,invoke_case<false, int, foo const*>
        ,invoke_case<false, int, foo const&>
        ,invoke_case<false, int, foo const&&>
        ,invoke_case<false, int, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();

// MSVC doesn't handle cv + ref qualifiers in expression sfinae correctly
#ifndef BOOST_CLBL_TRTS_MSVC

    run_tests<void(foo::*)() const LREF
        ,invoke_case<false, void, foo>
        ,invoke_case<true, void, foo*>
        ,invoke_case<true, void, foo&>
        ,invoke_case<false, int, foo*>
        ,invoke_case<false, int, foo&>
        ,invoke_case<false, void, foo&&>
        ,invoke_case<true, void, std::reference_wrapper<foo>>
        ,invoke_case<false, int, std::reference_wrapper<foo>>
        ,invoke_case<false, void, foo const>
        ,invoke_case<true, void, foo const*>
        ,invoke_case<true, void, foo const&>
        ,invoke_case<false, int, foo const*>
        ,invoke_case<false, int, foo const&>
        ,invoke_case<false, void, foo const&&>
        ,invoke_case<true, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, int, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();

    run_tests<void(foo::*)() const RREF
        ,invoke_case<true, void, foo>
        ,invoke_case<false, int, foo>
        ,invoke_case<false, void, foo*>
        ,invoke_case<false, void, foo&>
        ,invoke_case<true, void, foo&&>
        ,invoke_case<false, int, foo&&>
        ,invoke_case<false, void, std::reference_wrapper<foo>>
        ,invoke_case<true, void, foo const>
        ,invoke_case<false, int, foo const>
        ,invoke_case<false, void, foo const*>
        ,invoke_case<false, void, foo const&>
        ,invoke_case<true, void, foo const&&>
        ,invoke_case<false, int, foo const&&>
        ,invoke_case<false, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();

#endif // #ifndef BOOST_CLBL_TRTS_MSVC

    run_tests<int
        ,invoke_case<false, void, foo>
        ,invoke_case<false, void, foo*>
        ,invoke_case<false, void, foo&>
        ,invoke_case<false, void, foo&&>
        ,invoke_case<false, void, std::reference_wrapper<foo>>
        ,invoke_case<false, void, foo const>
        ,invoke_case<false, void, foo const*>
        ,invoke_case<false, void, foo const&>
        ,invoke_case<false, void, foo const&&>
        ,invoke_case<false, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();

    auto f = [](int){};

    run_tests<decltype(f)
        ,invoke_case<true, void, int>
        ,invoke_case<true, void, char>
        ,invoke_case<false, int, int>
        ,invoke_case<false, int, char>
        ,invoke_case<false, void, void*>
    >();

    auto g = [](){};

    run_tests<decltype(g)
        ,invoke_case<true, void>
        ,invoke_case<true, void>
        ,invoke_case<false, void, int>
        ,invoke_case<false, void, char>
        ,invoke_case<false, int, int>
        ,invoke_case<false, int, char>
        ,invoke_case<false, void, void*>
    >();

// libc++ requires constructible types be passed to std::is_invocable
#ifndef  _LIBCPP_VERSION

    run_tests<void(int)
        ,invoke_case<true, void, int>
        ,invoke_case<true, void, char>
        ,invoke_case<false, int, int>
        ,invoke_case<false, int, char>
        ,invoke_case<false, void, void*>
    >();

    run_tests<void()
        ,invoke_case<false, void, int>
        ,invoke_case<false, void, char>
        ,invoke_case<false, void, void*>
    >();

    run_tests<void
        ,invoke_case<false, void, foo>
        ,invoke_case<false, void, foo*>
        ,invoke_case<false, void, foo&>
        ,invoke_case<false, void, foo&&>
        ,invoke_case<false, void, std::reference_wrapper<foo>>
        ,invoke_case<false, void, foo const>
        ,invoke_case<false, void, foo const*>
        ,invoke_case<false, void, foo const&>
        ,invoke_case<false, void, foo const&&>
        ,invoke_case<false, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();
#endif

    run_tests<int
        ,invoke_case<false, void, foo>
        ,invoke_case<false, void, foo*>
        ,invoke_case<false, void, foo&>
        ,invoke_case<false, void, foo&&>
        ,invoke_case<false, void, std::reference_wrapper<foo>>
        ,invoke_case<false, void, foo const>
        ,invoke_case<false, void, foo const*>
        ,invoke_case<false, void, foo const&>
        ,invoke_case<false, void, foo const&&>
        ,invoke_case<false, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();

    run_tests<void*
        ,invoke_case<false, void, foo>
        ,invoke_case<false, void, foo*>
        ,invoke_case<false, void, foo&>
        ,invoke_case<false, void, foo&&>
        ,invoke_case<false, void, std::reference_wrapper<foo>>
        ,invoke_case<false, void, foo const>
        ,invoke_case<false, void, foo const*>
        ,invoke_case<false, void, foo const&>
        ,invoke_case<false, void, foo const&&>
        ,invoke_case<false, void, std::reference_wrapper<foo const>>
        ,invoke_case<false, void, foo, int>
        ,invoke_case<false, void, foo*, int>
        ,invoke_case<false, void, foo&, int>
        ,invoke_case<false, void, foo&&, int>
        ,invoke_case<false, void, std::reference_wrapper<foo>, int>
    >();
}

#endif //#ifdef BOOST_CLBL_TRTS_GCC_OLDER_THAN_4_9_2
