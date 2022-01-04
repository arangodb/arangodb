// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See benchmark.md

#ifndef BENCHMARK_WHAT
#   define BENCHMARK_WHAT 0
#endif

#if BENCHMARK_WHAT == 0

#   ifndef TL_EXPECTED_HPP
#       include "tl/expected.hpp"
#   endif
#   define BENCHMARK_SUCCESS(e) e
#   define BENCHMARK_FAILURE(e) tl::make_unexpected(e)
#   define BENCHMARK_TRY(v,r)\
        auto && _r_##v = r;\
        if( !_r_##v )\
            return BENCHMARK_FAILURE(_r_##v.error());\
    auto && v = _r_##v.value()

#else

#   include <boost/outcome/std_outcome.hpp>
#   include <boost/outcome/try.hpp>
#   define BENCHMARK_SUCCESS(e) boost::outcome_v2::success(e)
#   define BENCHMARK_FAILURE(e) boost::outcome_v2::failure(e)
#   define BENCHMARK_TRY BOOST_OUTCOME_TRY
#   ifndef BOOST_NO_EXCEPTIONS
#       error Please disable exception handling.
#   endif

#endif

#ifdef _MSC_VER
#   define NOINLINE __declspec(noinline)
#   define ALWAYS_INLINE __forceinline
#else
#   define NOINLINE __attribute__((noinline))
#   define ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <system_error>
#include <array>

namespace boost
{
    void throw_exception( std::exception const & e )
    {
        std::cerr << "Terminating due to a C++ exception under BOOST_NO_EXCEPTIONS: " << e.what();
        std::terminate();
    }

    struct source_location;
    void throw_exception( std::exception const & e, boost::source_location const & )
    {
        throw_exception(e);
    }
}

//////////////////////////////////////

#if BENCHMARK_WHAT == 0 // tl::expected

#   define USING_RESULT_TYPE "tl::expected<T, E>"

    template <class T, class E>
    using result = tl::expected<T, E>;

#elif BENCHMARK_WHAT == 1 // outcome::result

#   define USING_RESULT_TYPE "outcome::result<T, E>"

    template <class T, class E>
    using result = boost::outcome_v2::std_result<T, E, boost::outcome_v2::policy::terminate>;

#elif BENCHMARK_WHAT == 2 // outcome::outcome

#   define USING_RESULT_TYPE "outcome::outcome<T, E>"

    template <class T, class E>
    using result = boost::outcome_v2::std_outcome<T, E>;

#else
#   error Benchmark what?
#endif

//////////////////////////////////////

enum class e_error_code
{
    ec0, ec1, ec2, ec3
};

struct e_system_error
{
    int value;
    std::string what;
};

struct e_heavy_payload
{
    std::array<char, 4096> value;
};

template <class E>
E make_error() noexcept;

template <>
inline e_error_code make_error<e_error_code>() noexcept
{
    switch(std::rand()%4)
    {
        default: return e_error_code::ec0;
        case 1: return e_error_code::ec1;
        case 2: return e_error_code::ec2;
        case 3: return e_error_code::ec3;
    }
}

template <>
inline std::error_code make_error<std::error_code>() noexcept
{
    return std::error_code(std::rand(), std::system_category());
}

template <>
inline e_system_error make_error<e_system_error>() noexcept
{
    return { std::rand(), std::string(std::rand()%32, ' ') };
}

template <>
inline e_heavy_payload make_error<e_heavy_payload>() noexcept
{
    e_heavy_payload e;
    std::fill(e.value.begin(), e.value.end(), std::rand());
    return e;
}

inline bool should_fail( int failure_rate ) noexcept
{
    assert(failure_rate>=0);
    assert(failure_rate<=100);
    return (std::rand()%100) < failure_rate;
}

inline int handle_error( e_error_code e ) noexcept
{
    return int(e);
}

inline int handle_error( std::error_code const & e ) noexcept
{
    return e.value();
}

inline int handle_error( e_system_error const & e ) noexcept
{
    return e.value + e.what.size();
}

inline int handle_error( e_heavy_payload const & e ) noexcept
{
    return std::accumulate(e.value.begin(), e.value.end(), 0);
}

//////////////////////////////////////

// This is used to change the "success" type at each level.
// Generally, functions return values of different types.
template <int N, class E, bool Odd = N%2>
struct select_result_type;

template <int N, class E>
struct select_result_type<N, E, true>
{
    using type = result<int, E>;
};

template <int N, class E>
struct select_result_type<N, E, false>
{
    using type = result<float, E>;
};

template <int N, class E>
using select_result_t = typename select_result_type<N, E>::type;

//////////////////////////////////////

template <int N, class E>
struct benchmark
{
    using e_type = E;

    NOINLINE static select_result_t<N, E> f( int failure_rate ) noexcept
    {
        BENCHMARK_TRY(x, (benchmark<N-1, E>::f(failure_rate)));
        return BENCHMARK_SUCCESS(x+1);
    }
};

template <class E>
struct benchmark<1, E>
{
    using e_type = E;

    NOINLINE static select_result_t<1, E> f( int failure_rate ) noexcept
    {
        if( should_fail(failure_rate) )
            return BENCHMARK_FAILURE(make_error<E>());
        else
            return BENCHMARK_SUCCESS(std::rand());
    }
};

//////////////////////////////////////

template <class Benchmark>
NOINLINE int runner( int failure_rate ) noexcept
{
    if( auto r = Benchmark::f(failure_rate) )
        return r.value();
    else
        return handle_error(r.error());
}

//////////////////////////////////////

std::fstream append_csv()
{
    if( FILE * f = fopen("benchmark.csv","rb") )
    {
        fclose(f);
        return std::fstream("benchmark.csv", std::fstream::out | std::fstream::app);
    }
    else
    {
        std::fstream fs("benchmark.csv", std::fstream::out | std::fstream::app);
        fs << "\"Result Type\",2%,98%\n";
        return fs;
    }
}

template <class F>
int print_elapsed_time( int iteration_count, F && f )
{
    auto start = std::chrono::steady_clock::now();
    int val = 0;
    for( int i = 0; i!=iteration_count; ++i )
        val += std::forward<F>(f)();
    auto stop = std::chrono::steady_clock::now();
    int elapsed = std::chrono::duration_cast<std::chrono::microseconds>(stop-start).count();
    std::cout << std::right << std::setw(9) << elapsed;
    append_csv() << ',' << elapsed;
    return val;
}

//////////////////////////////////////

template <int Depth, class E>
int benchmark_type( char const * type_name, int iteration_count )
{
    int x=0;
    append_csv() << "\"" USING_RESULT_TYPE "\"";
    std::cout << '\n' << std::left << std::setw(16) << type_name << '|';
    std::srand(0);
    x += print_elapsed_time( iteration_count, [] { return runner<benchmark<Depth, E>>(2); } );
    std::cout << " |";
    std::srand(0);
    x += print_elapsed_time( iteration_count, [] { return runner<benchmark<Depth, E>>(98); } );
    append_csv() << '\n';
    return x;
}

//////////////////////////////////////

int main()
{
    int const depth = 10;
    int const iteration_count = 10000000;
    std::cout <<
        iteration_count << " iterations, call depth " << depth << ", sizeof(e_heavy_payload) = " << sizeof(e_heavy_payload) << "\n"
        USING_RESULT_TYPE "\n"
        "Error type      |  2% (μs) | 98% (μs)\n"
        "----------------|----------|---------";
    int r = 0;
    r += benchmark_type<depth, e_error_code>("e_error_code", iteration_count);
    r += benchmark_type<depth, std::error_code>("std::error_code", iteration_count);
    r += benchmark_type<depth, e_system_error>("e_system_error", iteration_count);
    r += benchmark_type<depth, e_heavy_payload>("e_heavy_payload", iteration_count);
    std::cout << '\n';
    // std::cout << std::rand() << '\n';
    return r;
}
