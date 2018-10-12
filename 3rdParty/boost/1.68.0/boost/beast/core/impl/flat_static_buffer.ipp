//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_FLAT_STATIC_BUFFER_IPP
#define BOOST_BEAST_IMPL_FLAT_STATIC_BUFFER_IPP

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace boost {
namespace beast {

/*  Memory is laid out thusly:

    begin_ ..|.. in_ ..|.. out_ ..|.. last_ ..|.. end_
*/

inline
auto
flat_static_buffer_base::
data() const ->
    const_buffers_type
{
    return {in_, dist(in_, out_)};
}

inline
void
flat_static_buffer_base::
reset()
{
    reset_impl();
}

inline
auto
flat_static_buffer_base::
prepare(std::size_t n) ->
    mutable_buffers_type
{
    return prepare_impl(n);
}

inline
void
flat_static_buffer_base::
reset(void* p, std::size_t n)
{
    reset_impl(p, n);
}

template<class>
void
flat_static_buffer_base::
reset_impl()
{
    in_ = begin_;
    out_ = begin_;
    last_ = begin_;
}

template<class>
void
flat_static_buffer_base::
reset_impl(void* p, std::size_t n)
{
    begin_ = static_cast<char*>(p);
    in_ = begin_;
    out_ = begin_;
    last_ = begin_;
    end_ = begin_ + n;
}

template<class>
auto
flat_static_buffer_base::
prepare_impl(std::size_t n) ->
    mutable_buffers_type
{
    if(n <= dist(out_, end_))
    {
        last_ = out_ + n;
        return {out_, n};
    }
    auto const len = size();
    if(n > capacity() - len)
        BOOST_THROW_EXCEPTION(std::length_error{
            "buffer overflow"});
    if(len > 0)
        std::memmove(begin_, in_, len);
    in_ = begin_;
    out_ = in_ + len;
    last_ = out_ + n;
    return {out_, n};
}

template<class>
void
flat_static_buffer_base::
consume_impl(std::size_t n)
{
    if(n >= size())
    {
        in_ = begin_;
        out_ = in_;
        return;
    }
    in_ += n;
}

//------------------------------------------------------------------------------

template<std::size_t N>
flat_static_buffer<N>::
flat_static_buffer(flat_static_buffer const& other)
    : flat_static_buffer_base(buf_, N)
{
    using boost::asio::buffer_copy;
    this->commit(buffer_copy(
        this->prepare(other.size()), other.data()));
}

template<std::size_t N>
auto
flat_static_buffer<N>::
operator=(flat_static_buffer const& other) ->
    flat_static_buffer<N>&
{
    using boost::asio::buffer_copy;
    this->consume(this->size());
    this->commit(buffer_copy(
        this->prepare(other.size()), other.data()));
    return *this;
}

} // beast
} // boost

#endif
