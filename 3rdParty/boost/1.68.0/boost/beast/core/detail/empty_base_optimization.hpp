//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_EMPTY_BASE_OPTIMIZATION_HPP
#define BOOST_BEAST_DETAIL_EMPTY_BASE_OPTIMIZATION_HPP

#include <boost/type_traits/is_final.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

template<class T>
struct is_empty_base_optimization_derived
    : std::integral_constant<bool,
        std::is_empty<T>::value &&
        ! boost::is_final<T>::value>
{
};

template<class T, int UniqueID = 0,
    bool isDerived =
        is_empty_base_optimization_derived<T>::value>
class empty_base_optimization : private T
{
public:
    empty_base_optimization() = default;
    empty_base_optimization(empty_base_optimization&&) = default;
    empty_base_optimization(empty_base_optimization const&) = default;
    empty_base_optimization& operator=(empty_base_optimization&&) = default;
    empty_base_optimization& operator=(empty_base_optimization const&) = default;

    template<class Arg1, class... ArgN>
    explicit
    empty_base_optimization(Arg1&& arg1, ArgN&&... argn)
        : T(std::forward<Arg1>(arg1),
            std::forward<ArgN>(argn)...)
    {
    }

    T& member() noexcept
    {
        return *this;
    }

    T const& member() const noexcept
    {
        return *this;
    }
};

//------------------------------------------------------------------------------

template<
    class T,
    int UniqueID
>
class empty_base_optimization <T, UniqueID, false>
{
    T t_;

public:
    empty_base_optimization() = default;
    empty_base_optimization(empty_base_optimization&&) = default;
    empty_base_optimization(empty_base_optimization const&) = default;
    empty_base_optimization& operator=(empty_base_optimization&&) = default;
    empty_base_optimization& operator=(empty_base_optimization const&) = default;

    template<class Arg1, class... ArgN>
    explicit
    empty_base_optimization(Arg1&& arg1, ArgN&&... argn)
        : t_(std::forward<Arg1>(arg1),
            std::forward<ArgN>(argn)...)
    {
    }

    T& member() noexcept
    {
        return t_;
    }

    T const& member() const noexcept
    {
        return t_;
    }
};

} // detail
} // beast
} // boost

#endif
