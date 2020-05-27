// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <chrono>
#include <iostream>

#include <benchmark/benchmark.h>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


//[ arithmetic_perf_decls
namespace user {

    struct number
    {
        double value;

        friend number operator+(number lhs, number rhs)
        {
            return number{lhs.value + rhs.value};
        }

        friend number operator*(number lhs, number rhs)
        {
            return number{lhs.value * rhs.value};
        }
    };
}
//]

double get_noise()
{
    auto const start_time = std::chrono::high_resolution_clock::now();
    auto const start_time_ns =
        std::chrono::time_point_cast<std::chrono::nanoseconds>(start_time);
    return 1.0 * start_time_ns.time_since_epoch().count();
}


user::number g_a{get_noise()};
user::number g_x{get_noise()};
user::number g_y{get_noise()};

//[ arithmetic_perf_eval_as_yap_expr
user::number eval_as_yap_expr(user::number a_, user::number x_, user::number y_)
{
    term<user::number> a{{a_}};
    term<user::number> x{{x_}};
    term<user::number> y{{y_}};
    auto expr = (a * x + y) * (a * x + y) + (a * x + y);
    return yap::evaluate(expr);
}
//]

void BM_eval_as_yap_expr(benchmark::State & state)
{
    double d = 0;
    while (state.KeepRunning()) {
        user::number const n = eval_as_yap_expr(g_a, g_x, g_y);
        d += n.value;
    }
    std::cout << "Sum of doubles=" << d << "\n";
}

//[ arithmetic_perf_eval_as_yap_expr_4x
user::number
eval_as_yap_expr_4x(user::number a_, user::number x_, user::number y_)
{
    term<user::number> a{{a_}};
    term<user::number> x{{x_}};
    term<user::number> y{{y_}};
    auto expr = (a * x + y) * (a * x + y) + (a * x + y) +
                (a * x + y) * (a * x + y) + (a * x + y) +
                (a * x + y) * (a * x + y) + (a * x + y) +
                (a * x + y) * (a * x + y) + (a * x + y);
    return yap::evaluate(expr);
}
//]

void BM_eval_as_yap_expr_4x(benchmark::State & state)
{
    double d = 0;
    while (state.KeepRunning()) {
        user::number const n = eval_as_yap_expr_4x(g_a, g_x, g_y);
        d += n.value;
    }
    std::cout << "Sum of doubles=" << d << "\n";
}

//[ arithmetic_perf_eval_as_cpp_expr
user::number eval_as_cpp_expr(user::number a, user::number x, user::number y)
{
    return (a * x + y) * (a * x + y) + (a * x + y);
}
//]

void BM_eval_as_cpp_expr(benchmark::State & state)
{
    double d = 0;
    while (state.KeepRunning()) {
        user::number const n = eval_as_cpp_expr(g_a, g_x, g_y);
        d += n.value;
    }
    std::cout << "Sum of doubles=" << d << "\n";
}

//[ arithmetic_perf_eval_as_cpp_expr_4x
user::number eval_as_cpp_expr_4x(user::number a, user::number x, user::number y)
{
    return (a * x + y) * (a * x + y) + (a * x + y) + (a * x + y) * (a * x + y) +
           (a * x + y) + (a * x + y) * (a * x + y) + (a * x + y) +
           (a * x + y) * (a * x + y) + (a * x + y);
}
//]

void BM_eval_as_cpp_expr_4x(benchmark::State & state)
{
    double d = 0;
    while (state.KeepRunning()) {
        user::number const n = eval_as_cpp_expr_4x(g_a, g_x, g_y);
        d += n.value;
    }
    std::cout << "Sum of doubles=" << d << "\n";
}

BENCHMARK(BM_eval_as_yap_expr);
BENCHMARK(BM_eval_as_yap_expr_4x);
BENCHMARK(BM_eval_as_cpp_expr);
BENCHMARK(BM_eval_as_cpp_expr_4x);

BENCHMARK_MAIN()
