/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    static_def.hpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#ifndef GUARD_STATIC_DEF
#define GUARD_STATIC_DEF

#include <boost/hof/function.hpp>
#include <boost/hof/lambda.hpp>

// MSVC seems to not support unique addressing at all
#if defined (_MSC_VER)
#define BOOST_HOF_HAS_UNIQUE_STATIC_VAR 0
#define BOOST_HOF_HAS_UNIQUE_STATIC_LAMBDA_FUNCTION_ADDR 0
// Gcc 4.6 only supports unique addressing for non-lambdas
#elif defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ < 7
#define BOOST_HOF_HAS_UNIQUE_STATIC_VAR 1
#define BOOST_HOF_HAS_UNIQUE_STATIC_LAMBDA_FUNCTION_ADDR 0
#else
#define BOOST_HOF_HAS_UNIQUE_STATIC_VAR 1
#define BOOST_HOF_HAS_UNIQUE_STATIC_LAMBDA_FUNCTION_ADDR 1
#endif

namespace fit_test {

BOOST_HOF_STATIC_LAMBDA_FUNCTION(fit_sum_lambda) = [](int x, int y) 
{
    return x + y;
};

struct fit_sum_f
{
    constexpr int operator()(int x, int y) const
    {
        return x + y;
    }
};

BOOST_HOF_STATIC_LAMBDA_FUNCTION(fit_sum_fo) = fit_sum_f();

BOOST_HOF_STATIC_FUNCTION(fit_sum_constexpr_fo) = fit_sum_f();

BOOST_HOF_DECLARE_STATIC_VAR(fit_sum_var, fit_sum_f);

// BOOST_HOF_STATIC_FUNCTION(fit_sum) = [](auto x, auto y) 
// {
//     return x + y;
// };

template<class T>
T fit_sum(T x, T y) 
{
    return x + y;
};

}

#endif
