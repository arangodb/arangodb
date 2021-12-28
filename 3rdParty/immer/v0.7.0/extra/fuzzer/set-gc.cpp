//
// immer: immutable data structures for C++
// Copyright (C) 2016, 2017, 2018 Juan Pedro Bolivar Puente
//
// This software is distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt
//

#include "fuzzer_gc_guard.hpp"
#include "fuzzer_input.hpp"

#include <immer/heap/gc_heap.hpp>
#include <immer/refcount/no_refcount_policy.hpp>
#include <immer/set.hpp>

#include <immer/algorithm.hpp>

#include <array>

using gc_memory = immer::memory_policy<immer::heap_policy<immer::gc_heap>,
                                       immer::no_refcount_policy,
                                       immer::default_lock_policy,
                                       immer::gc_transience_policy,
                                       false>;

struct colliding_hash_t
{
    std::size_t operator()(std::size_t x) const { return x & ~15; }
};

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size)
{
    auto guard = fuzzer_gc_guard{};

    constexpr auto var_count = 4;

    using set_t =
        immer::set<size_t, colliding_hash_t, std::equal_to<>, gc_memory>;

    auto vars = std::array<set_t, var_count>{};

    auto is_valid_var = [&](auto idx) { return idx >= 0 && idx < var_count; };

    return fuzzer_input{data, size}.run([&](auto& in) {
        enum ops
        {
            op_insert,
            op_erase,
            op_insert_move,
            op_erase_move,
            op_iterate
        };
        auto src = read<char>(in, is_valid_var);
        auto dst = read<char>(in, is_valid_var);
        switch (read<char>(in)) {
        case op_insert: {
            auto value = read<size_t>(in);
            vars[dst]  = vars[src].insert(value);
            break;
        }
        case op_erase: {
            auto value = read<size_t>(in);
            vars[dst]  = vars[src].erase(value);
            break;
        }
        case op_insert_move: {
            auto value = read<size_t>(in);
            vars[dst]  = std::move(vars[src]).insert(value);
            break;
        }
        case op_erase_move: {
            auto value = read<size_t>(in);
            vars[dst]  = std::move(vars[src]).erase(value);
            break;
        }
        case op_iterate: {
            auto srcv = vars[src];
            immer::for_each(srcv, [&](auto&& v) {
                vars[dst] = std::move(vars[dst]).insert(v);
            });
            break;
        }
        default:
            break;
        };
        return true;
    });
}
