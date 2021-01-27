//
// immer: immutable data structures for C++
// Copyright (C) 2016, 2017, 2018 Juan Pedro Bolivar Puente
//
// This software is distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt
//

#pragma once

#include <stdexcept>
#include <cstdint>

struct no_more_input : std::exception {};

struct fuzzer_input
{
    const std::uint8_t* data_;
    std::size_t size_;

    const std::uint8_t* next(std::size_t size)
    {
        if (size_ < size) throw no_more_input{};
        auto r = data_;
        data_ += size;
        size_ -= size;
        return r;
    }

    const std::uint8_t* next(std::size_t size, std::size_t align)
    {
        auto rem = size % align;
        if (rem) next(align - rem);
        return next(size);
    }

    template <typename Fn>
    int run(Fn step)
    {
        try {
            while (step(*this));
        } catch (const no_more_input&) {};
        return 0;
    }
};

template <typename T>
const T& read(fuzzer_input& fz)
{
    return *reinterpret_cast<const T*>(fz.next(sizeof(T), alignof(T)));
}

template <typename T, typename Cond>
T read(fuzzer_input& fz, Cond cond)
{
    auto x = read<T>(fz);
    return cond(x)
        ? x
        : read<T>(fz, cond);
}
