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
//gcc >= 4.8 doesn't like the invoke_case pattern used here
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

// MSVC doesn't handle cv + ref qualifiers in expression sfinae correctly
#ifndef BOOST_CLBL_TRTS_MSVC

    run_tests<void(foo::*)() const LREF
        ,invoke_case<false, foo>
        ,invoke_case<true, foo*>
        ,invoke_case<true, foo&>
        ,invoke_case<false, foo&&>
        ,invoke_case<true, std::reference_wrapper<foo>>
        ,invoke_case<false, foo const>
        ,invoke_case<true, foo const*>
        ,invoke_case<true, foo const&>
        ,invoke_case<false, foo const&&>
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

#endif // #ifndef BOOST_CLBL_TRTS_MSVC

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
