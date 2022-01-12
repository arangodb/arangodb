/*
Copyright Barrett Adair 2016-2021
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

template<bool Expect, typename... Args>
struct invoke_case {
   template<typename Callable>
   void operator()(tag<Callable>) const {

       CT_ASSERT((Expect == boost::callable_traits::is_invocable<Callable, Args...>()));
#ifndef BOOST_CLBL_TRTS_DISABLE_VARIABLE_TEMPLATES
       CT_ASSERT((Expect == boost::callable_traits::is_invocable_v<Callable, Args...>));
#endif

// when available, test parity with std implementation
#if defined(__cpp_lib_is_invocable)
       CT_ASSERT((std::is_invocable<Callable, Args...>() == boost::callable_traits::is_invocable<Callable, Args...>()));
#  ifndef BOOST_CLBL_TRTS_DISABLE_VARIABLE_TEMPLATES
       CT_ASSERT((std::is_invocable_v<Callable, Args...> == boost::callable_traits::is_invocable_v<Callable, Args...>));
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
        ,invoke_case<true, foo>
        ,invoke_case<true, foo*>
        ,invoke_case<true, foo&>
        ,invoke_case<true, foo&&>
        ,invoke_case<true, std::reference_wrapper<foo>>
        ,invoke_case<false, foo const>
        ,invoke_case<false, foo const*>
        ,invoke_case<false, foo const&>
        ,invoke_case<false, foo const&&>
        ,invoke_case<false, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();

    run_tests<void(foo::*)() LREF
        ,invoke_case<false, foo>
        ,invoke_case<true, foo*>
        ,invoke_case<true, foo&>
        ,invoke_case<false, foo&&>
        ,invoke_case<true, std::reference_wrapper<foo>>
        ,invoke_case<false, foo const>
        ,invoke_case<false, foo const*>
        ,invoke_case<false, foo const&>
        ,invoke_case<false, foo const&&>
        ,invoke_case<false, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();

    run_tests<void(foo::*)() RREF
        ,invoke_case<true, foo>
        ,invoke_case<false, foo*>
        ,invoke_case<false, foo&>
        ,invoke_case<true, foo&&>
        ,invoke_case<false, std::reference_wrapper<foo>>
        ,invoke_case<false, foo const>
        ,invoke_case<false, foo const*>
        ,invoke_case<false, foo const&>
        ,invoke_case<false, foo const&&>
        ,invoke_case<false, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();

    run_tests<void(foo::*)() const
        ,invoke_case<true, foo>
        ,invoke_case<true, foo*>
        ,invoke_case<true, foo&>
        ,invoke_case<true, foo&&>
        ,invoke_case<true, std::reference_wrapper<foo>>
        ,invoke_case<true, foo const>
        ,invoke_case<true, foo const*>
        ,invoke_case<true, foo const&>
        ,invoke_case<true, foo const&&>
        ,invoke_case<true, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
            ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();

// old MSVC doesn't handle cv + ref qualifiers in expression sfinae correctly
#ifndef BOOST_CLBL_TRTS_OLD_MSVC

#if __cplusplus <= 201703L
#define QUIRKY_CASE true
#else
#define QUIRKY_CASE false
#endif

    run_tests<void(foo::*)() const LREF

#ifndef BOOST_CLBL_TRTS_MSVC
        ,invoke_case<!QUIRKY_CASE, foo>
        ,invoke_case<!QUIRKY_CASE, foo&&>
        ,invoke_case<!QUIRKY_CASE, foo const>
        ,invoke_case<!QUIRKY_CASE, foo const&&>
#endif
        ,invoke_case<true, foo*>
        ,invoke_case<true, foo&>
        ,invoke_case<true, std::reference_wrapper<foo>>
        ,invoke_case<true, foo const*>
        ,invoke_case<true, foo const&>
        ,invoke_case<true, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();

    run_tests<void(foo::*)() const RREF
        ,invoke_case<true, foo>
        ,invoke_case<false, foo*>
        ,invoke_case<false, foo&>
        ,invoke_case<true, foo&&>
        ,invoke_case<false, std::reference_wrapper<foo>>
        ,invoke_case<true, foo const>
        ,invoke_case<false, foo const*>
        ,invoke_case<false, foo const&>
        ,invoke_case<true, foo const&&>
        ,invoke_case<false, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();

#endif // #ifndef BOOST_CLBL_TRTS_OLD_MSVC

    run_tests<int
        ,invoke_case<false, foo>
        ,invoke_case<false, foo*>
        ,invoke_case<false, foo&>
        ,invoke_case<false, foo&&>
        ,invoke_case<false, std::reference_wrapper<foo>>
        ,invoke_case<false, foo const>
        ,invoke_case<false, foo const*>
        ,invoke_case<false, foo const&>
        ,invoke_case<false, foo const&&>
        ,invoke_case<false, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();

    auto f = [](int){};

    run_tests<decltype(f)
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(f)&
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(f))
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(f))&
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(f))&&
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(f)) const &
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(f)) const &&
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(f)&&
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(f) const &
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(f) const &&
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    auto g = [](){};

    run_tests<decltype(g)
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(g)&
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(g))
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(g))&
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(g))&&
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(g)) const &
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(std::ref(g)) const &&
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(g)&&
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(g) const &
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<decltype(g) const &&
        ,invoke_case<true>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

// libc++ requires constructible types be passed to std::is_invocable
#ifndef  _LIBCPP_VERSION

    run_tests<void(int)
        ,invoke_case<true, int>
        ,invoke_case<true, char>
        ,invoke_case<false, void*>
    >();

    run_tests<void(int) const
        ,invoke_case<false, int>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<void()
        ,invoke_case<false, int>
        ,invoke_case<false, char>
        ,invoke_case<false, void*>
    >();

    run_tests<void
        ,invoke_case<false, foo>
        ,invoke_case<false, foo*>
        ,invoke_case<false, foo&>
        ,invoke_case<false, foo&&>
        ,invoke_case<false, std::reference_wrapper<foo>>
        ,invoke_case<false, foo const>
        ,invoke_case<false, foo const*>
        ,invoke_case<false, foo const&>
        ,invoke_case<false, foo const&&>
        ,invoke_case<false, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();
#endif

    run_tests<int
        ,invoke_case<false, foo>
        ,invoke_case<false, foo*>
        ,invoke_case<false, foo&>
        ,invoke_case<false, foo&&>
        ,invoke_case<false, std::reference_wrapper<foo>>
        ,invoke_case<false, foo const>
        ,invoke_case<false, foo const*>
        ,invoke_case<false, foo const&>
        ,invoke_case<false, foo const&&>
        ,invoke_case<false, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();

    run_tests<void*
        ,invoke_case<false, foo>
        ,invoke_case<false, foo*>
        ,invoke_case<false, foo&>
        ,invoke_case<false, foo&&>
        ,invoke_case<false, std::reference_wrapper<foo>>
        ,invoke_case<false, foo const>
        ,invoke_case<false, foo const*>
        ,invoke_case<false, foo const&>
        ,invoke_case<false, foo const&&>
        ,invoke_case<false, std::reference_wrapper<foo const>>
        ,invoke_case<false, foo, int>
        ,invoke_case<false, foo*, int>
        ,invoke_case<false, foo&, int>
        ,invoke_case<false, foo&&, int>
        ,invoke_case<false, std::reference_wrapper<foo>, int>
    >();
}

#endif //#ifdef BOOST_CLBL_TRTS_GCC_OLDER_THAN_4_9_2
