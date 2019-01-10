//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_BUFFERS_CAT_IPP
#define BOOST_BEAST_IMPL_BUFFERS_CAT_IPP

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/beast/core/detail/variant.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/throw_exception.hpp>
#include <cstdint>
#include <iterator>
#include <new>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace boost {
namespace beast {

template<class... Bn>
class buffers_cat_view<Bn...>::const_iterator
{
    // VFALCO The logic to skip empty sequences fails
    //        if there is just one buffer in the list.
    static_assert(sizeof...(Bn) >= 2,
        "A minimum of two sequences are required");

    struct past_end
    {
        char unused = 0; // make g++8 happy

        operator bool() const noexcept
        {
            return true;
        }
    };

    std::tuple<Bn...> const* bn_ = nullptr;
    detail::variant<typename
        detail::buffer_sequence_iterator<Bn>::type...,
            past_end> it_;

    friend class buffers_cat_view<Bn...>;

    template<std::size_t I>
    using C = std::integral_constant<std::size_t, I>;

public:
    using value_type = typename
        detail::common_buffers_type<Bn...>::type;
    using pointer = value_type const*;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    const_iterator() = default;
    const_iterator(const_iterator&& other) = default;
    const_iterator(const_iterator const& other) = default;
    const_iterator& operator=(const_iterator&& other) = default;
    const_iterator& operator=(const_iterator const& other) = default;

    bool
    operator==(const_iterator const& other) const;

    bool
    operator!=(const_iterator const& other) const
    {
        return ! (*this == other);
    }

    reference
    operator*() const;

    pointer
    operator->() const = delete;

    const_iterator&
    operator++();

    const_iterator
    operator++(int);

    // deprecated
    const_iterator&
    operator--();

    // deprecated
    const_iterator
    operator--(int);

private:
    const_iterator(
        std::tuple<Bn...> const& bn, bool at_end);

    template<std::size_t I>
    void
    construct(C<I> const&)
    {
        if(boost::asio::buffer_size(
            std::get<I>(*bn_)) != 0)
        {
            it_.template emplace<I+1>(
                boost::asio::buffer_sequence_begin(
                    std::get<I>(*bn_)));
            return;
        }
        construct(C<I+1>{});
    }

    void
    construct(C<sizeof...(Bn)-1> const&)
    {
        auto constexpr I = sizeof...(Bn)-1;
        it_.template emplace<I+1>(
            boost::asio::buffer_sequence_begin(
                std::get<I>(*bn_)));
    }

    void
    construct(C<sizeof...(Bn)> const&)
    {
        // end
        auto constexpr I = sizeof...(Bn);
        it_.template emplace<I+1>();
    }

    template<std::size_t I>
    void
    next(C<I> const&)
    {
        if(boost::asio::buffer_size(
            std::get<I>(*bn_)) != 0)
        {
            it_.template emplace<I+1>(
                boost::asio::buffer_sequence_begin(
                    std::get<I>(*bn_)));
            return;
        }
        next(C<I+1>{});
    }

    void
    next(C<sizeof...(Bn)> const&)
    {
        // end
        auto constexpr I = sizeof...(Bn);
        it_.template emplace<I+1>();
    }

    template<std::size_t I>
    void
    prev(C<I> const&)
    {
        if(boost::asio::buffer_size(
            std::get<I>(*bn_)) != 0)
        {
            it_.template emplace<I+1>(
                boost::asio::buffer_sequence_end(
                    std::get<I>(*bn_)));
            return;
        }
        prev(C<I-1>{});
    }

    void
    prev(C<0> const&)
    {
        auto constexpr I = 0;
        it_.template emplace<I+1>(
            boost::asio::buffer_sequence_end(
                std::get<I>(*bn_)));
    }

    template<std::size_t I>
    reference
    dereference(C<I> const&) const
    {
        if(it_.index() == I+1)
            return *it_.template get<I+1>();
        return dereference(C<I+1>{});
    }

    [[noreturn]]
    reference
    dereference(C<sizeof...(Bn)> const&) const
    {
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid iterator"});
    }

    template<std::size_t I>
    void
    increment(C<I> const&)
    {
        if(it_.index() == I+1)
        {
            if(++it_.template get<I+1>() !=
                boost::asio::buffer_sequence_end(
                    std::get<I>(*bn_)))
                return;
            return next(C<I+1>{});
        }
        increment(C<I+1>{});
    }

    [[noreturn]]
    void
    increment(C<sizeof...(Bn)> const&)
    {
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid iterator"});
    }

    void
    decrement(C<sizeof...(Bn)> const&)
    {
        auto constexpr I = sizeof...(Bn);
        if(it_.index() == I+1)
            prev(C<I-1>{});
        decrement(C<I-1>{});
    }

    template<std::size_t I>
    void
    decrement(C<I> const&)
    {
        if(it_.index() == I+1)
        {
            if(it_.template get<I+1>() !=
                boost::asio::buffer_sequence_begin(
                    std::get<I>(*bn_)))
            {
                --it_.template get<I+1>();
                return;
            }
            prev(C<I-1>{});
        }
        decrement(C<I-1>{});
    }

    void
    decrement(C<0> const&)
    {
        auto constexpr I = 0;
        if(it_.template get<I+1>() !=
            boost::asio::buffer_sequence_begin(
                std::get<I>(*bn_)))
        {
            --it_.template get<I+1>();
            return;
        }
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid iterator"});
    }
};

//------------------------------------------------------------------------------

template<class... Bn>
buffers_cat_view<Bn...>::
const_iterator::
const_iterator(
    std::tuple<Bn...> const& bn, bool at_end)
    : bn_(&bn)
{
    if(! at_end)
        construct(C<0>{});
    else
        construct(C<sizeof...(Bn)>{});
}

template<class... Bn>
bool
buffers_cat_view<Bn...>::
const_iterator::
operator==(const_iterator const& other) const
{
    return
        (bn_ == nullptr) ?
        (
            other.bn_ == nullptr ||
            other.it_.index() == sizeof...(Bn)
        ):(
            (other.bn_ == nullptr) ?
            (
                it_.index() == sizeof...(Bn)
            ): (
                bn_ == other.bn_ &&
                it_ == other.it_
            )
        );
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator*() const ->
    reference
{
    return dereference(C<0>{});
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator++() ->
    const_iterator&
{
    increment(C<0>{});
    return *this;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator++(int) ->
    const_iterator
{
    auto temp = *this;
    ++(*this);
    return temp;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator--() ->
    const_iterator&
{
    decrement(C<sizeof...(Bn)>{});
    return *this;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator--(int) ->
    const_iterator
{
    auto temp = *this;
    --(*this);
    return temp;
}

//------------------------------------------------------------------------------

template<class... Bn>
buffers_cat_view<Bn...>::
buffers_cat_view(Bn const&... bn)
    : bn_(bn...)
{
}


template<class... Bn>
inline
auto
buffers_cat_view<Bn...>::begin() const ->
    const_iterator
{
    return const_iterator{bn_, false};
}

template<class... Bn>
inline
auto
buffers_cat_view<Bn...>::end() const ->
    const_iterator
{
    return const_iterator{bn_, true};
}

} // beast
} // boost

#endif
