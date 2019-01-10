/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    is_invocable.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/is_invocable.hpp>
#include <ciso646>
#include "test.hpp"

template<int N>
struct callable_rank : callable_rank<N-1>
{};

template<>
struct callable_rank<0>
{};

BOOST_HOF_STATIC_TEST_CASE()
{
    struct is_callable_class
    {
        void operator()(int) const
        {
        }
    };
    struct callable_test_param {};

    void is_callable_function(int)
    {
    }

    struct is_callable_rank_class
    {
        void operator()(int, callable_rank<3>) const
        {
        }

        void operator()(int, callable_rank<4>) const
        {
        }
    };

    static_assert(boost::hof::is_invocable<is_callable_class, int>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_class, long>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_class, double>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_class, const int&>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_class, const long&>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_class, const double&>::value, "Not callable");
    static_assert(not boost::hof::is_invocable<is_callable_class, callable_test_param>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_class>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_class, int, int>::value, "callable failed");

    typedef void (*is_callable_function_pointer)(int);
    static_assert(boost::hof::is_invocable<is_callable_function_pointer, int>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_function_pointer, long>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_function_pointer, double>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_function_pointer, const int&>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_function_pointer, const long&>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_function_pointer, const double&>::value, "Not callable");
    static_assert(not boost::hof::is_invocable<is_callable_function_pointer, callable_test_param>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_function_pointer>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_function_pointer, int, int>::value, "callable failed");

    static_assert(boost::hof::is_invocable<is_callable_rank_class, int, callable_rank<3>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, long, callable_rank<3>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, double, callable_rank<3>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const int&, callable_rank<3>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const long&, callable_rank<3>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const double&, callable_rank<3>>::value, "Not callable");

    static_assert(boost::hof::is_invocable<is_callable_rank_class, int, callable_rank<4>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, long, callable_rank<4>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, double, callable_rank<4>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const int&, callable_rank<4>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const long&, callable_rank<4>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const double&, callable_rank<4>>::value, "Not callable");

    static_assert(boost::hof::is_invocable<is_callable_rank_class, int, callable_rank<5>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, long, callable_rank<5>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, double, callable_rank<5>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const int&, callable_rank<5>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const long&, callable_rank<5>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const double&, callable_rank<5>>::value, "Not callable");

    static_assert(boost::hof::is_invocable<is_callable_rank_class, int, callable_rank<6>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, long, callable_rank<6>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, double, callable_rank<6>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const int&, callable_rank<6>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const long&, callable_rank<6>>::value, "Not callable");
    static_assert(boost::hof::is_invocable<is_callable_rank_class, const double&, callable_rank<6>>::value, "Not callable");

    static_assert(not boost::hof::is_invocable<is_callable_rank_class, int, callable_rank<1>>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, long, callable_rank<1>>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, double, callable_rank<1>>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, const int&, callable_rank<1>>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, const long&, callable_rank<1>>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, const double&, callable_rank<1>>::value, "callable failed");

    static_assert(not boost::hof::is_invocable<is_callable_rank_class, callable_test_param, callable_test_param>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, callable_rank<3>, callable_test_param>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, callable_rank<4>, callable_test_param>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, callable_test_param, callable_rank<3>>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, callable_test_param, callable_rank<4>>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class>::value, "callable failed");
    static_assert(not boost::hof::is_invocable<is_callable_rank_class, int, int>::value, "callable failed");
};

BOOST_HOF_STATIC_TEST_CASE()
{
    typedef int(callable_rank<0>::*fn)(int);

    static_assert(boost::hof::is_invocable<fn, callable_rank<0>&, int>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, callable_rank<1>&, int>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fn, callable_rank<0>&>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fn, callable_rank<0> const&, int>::value, "Failed");
};

BOOST_HOF_STATIC_TEST_CASE()
{
    typedef int(callable_rank<0>::*fn)(int);

    typedef callable_rank<0>* T;
    typedef callable_rank<1>* DT;
    typedef const callable_rank<0>* CT;
    typedef std::unique_ptr<callable_rank<0>> ST;

    static_assert(boost::hof::is_invocable<fn, T&, int>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, DT&, int>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, const T&, int>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, T&&, int>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, ST, int>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fn, CT&, int>::value, "Failed");

};

BOOST_HOF_STATIC_TEST_CASE()
{
    typedef int(callable_rank<0>::*fn);

    static_assert(!boost::hof::is_invocable<fn>::value, "Failed");
};

BOOST_HOF_STATIC_TEST_CASE()
{
    typedef int(callable_rank<0>::*fn);

    static_assert(boost::hof::is_invocable<fn, callable_rank<0>&>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, callable_rank<0>&&>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, const callable_rank<0>&>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, callable_rank<1>&>::value, "Failed");
};

BOOST_HOF_STATIC_TEST_CASE()
{
    typedef int(callable_rank<0>::*fn);

    typedef callable_rank<0>* T;
    typedef callable_rank<1>* DT;
    typedef const callable_rank<0>* CT;
    typedef std::unique_ptr<callable_rank<0>> ST;

    static_assert(boost::hof::is_invocable<fn, T&>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, DT&>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, const T&>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, T&&>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, ST>::value, "Failed");
    static_assert(boost::hof::is_invocable<fn, CT&>::value, "Failed");

};

BOOST_HOF_STATIC_TEST_CASE()
{
    typedef void(*fp)(callable_rank<0>&, int);

    static_assert(boost::hof::is_invocable<fp, callable_rank<0>&, int>::value, "Failed");
    static_assert(boost::hof::is_invocable<fp, callable_rank<1>&, int>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fp, const callable_rank<0>&, int>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fp>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fp, callable_rank<0>&>::value, "Failed");
};

BOOST_HOF_STATIC_TEST_CASE()
{
    typedef void(&fp)(callable_rank<0>&, int);

    static_assert(boost::hof::is_invocable<fp, callable_rank<0>&, int>::value, "Failed");
    static_assert(boost::hof::is_invocable<fp, callable_rank<1>&, int>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fp, const callable_rank<0>&, int>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fp>::value, "Failed");
    static_assert(!boost::hof::is_invocable<fp, callable_rank<0>&>::value, "Failed");
};
