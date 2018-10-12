// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/assign/list_of.hpp>

#include <map>
#include <iostream>

#include <benchmark/benchmark.h>


template<typename Key, typename Value, typename Allocator>
struct map_list_of_transform
{
    template<typename Fn, typename Key2, typename Value2>
    auto operator()(
        boost::yap::expr_tag<boost::yap::expr_kind::call>,
        Fn const & fn,
        Key2 && key,
        Value2 && value)
    {
        boost::yap::transform(
            boost::yap::as_expr<boost::yap::minimal_expr>(fn), *this);
        map.emplace(
            Key{std::forward<Key2 &&>(key)},
            Value{std::forward<Value2 &&>(value)});
        return 0;
    }

    std::map<Key, Value, Allocator> map;
};


template<boost::yap::expr_kind Kind, typename Tuple>
struct map_list_of_expr
{
    static boost::yap::expr_kind const kind = Kind;

    Tuple elements;

    template<typename Key, typename Value, typename Allocator>
    operator std::map<Key, Value, Allocator>() const
    {
        map_list_of_transform<Key, Value, Allocator> transform;
        boost::yap::transform(*this, transform);
        return transform.map;
    }

    BOOST_YAP_USER_CALL_OPERATOR(::map_list_of_expr)
};

struct map_list_of_tag
{};

auto map_list_of =
    boost::yap::make_terminal<map_list_of_expr>(map_list_of_tag{});


std::map<std::string, int> make_map_with_boost_yap()
{
    return map_list_of("<", 1)("<=", 2)(">", 3)(">=", 4)("=", 5)("<>", 6);
}

void BM_make_map_with_boost_yap(benchmark::State & state)
{
    int i = 0;
    while (state.KeepRunning()) {
        {
            std::map<std::string, int> map = make_map_with_boost_yap();
            state.PauseTiming();
            for (auto && x : map) {
                i += x.second;
            }
        }
        state.ResumeTiming();
    }
    std::cout << "Sum of ints in all maps made=" << i << "\n";
}

std::map<std::string, int> make_map_with_boost_assign()
{
    return boost::assign::map_list_of("<", 1)("<=", 2)(">", 3)(">=", 4)("=", 5)(
        "<>", 6);
}

void BM_make_map_with_boost_assign(benchmark::State & state)
{
    int i = 0;
    while (state.KeepRunning()) {
        {
            std::map<std::string, int> map = make_map_with_boost_assign();
            state.PauseTiming();
            for (auto && x : map) {
                i += x.second;
            }
        }
        state.ResumeTiming();
    }
    std::cout << "Sum of ints in all maps made=" << i << "\n";
}

std::map<std::string, int> make_map_manually()
{
    std::map<std::string, int> retval;
    retval.emplace("<", 1);
    retval.emplace("<=", 2);
    retval.emplace(">", 3);
    retval.emplace(">=", 4);
    retval.emplace("=", 5);
    retval.emplace("<>", 6);
    return retval;
}

void BM_make_map_manually(benchmark::State & state)
{
    int i = 0;
    while (state.KeepRunning()) {
        {
            std::map<std::string, int> map = make_map_manually();
            state.PauseTiming();
            for (auto && x : map) {
                i += x.second;
            }
        }
        state.ResumeTiming();
    }
    std::cout << "Sum of ints in all maps made=" << i << "\n";
}

std::map<std::string, int> make_map_inializer_list()
{
    std::map<std::string, int> retval = {
        {"<", 1}, {"<=", 2}, {">", 3}, {">=", 4}, {"=", 5}, {"<>", 6}};
    return retval;
}

void BM_make_map_inializer_list(benchmark::State & state)
{
    int i = 0;
    while (state.KeepRunning()) {
        {
            std::map<std::string, int> map = make_map_inializer_list();
            state.PauseTiming();
            for (auto && x : map) {
                i += x.second;
            }
        }
        state.ResumeTiming();
    }
    std::cout << "Sum of ints in all maps made=" << i << "\n";
}

BENCHMARK(BM_make_map_with_boost_yap);
BENCHMARK(BM_make_map_with_boost_assign);
BENCHMARK(BM_make_map_manually);
BENCHMARK(BM_make_map_inializer_list);

BENCHMARK_MAIN()
