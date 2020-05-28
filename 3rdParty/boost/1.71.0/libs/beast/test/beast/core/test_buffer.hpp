//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_BUFFER_HPP
#define BOOST_BEAST_TEST_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <algorithm>
#include <string>
#include <type_traits>

namespace boost {
namespace beast {

/** A MutableBufferSequence for tests, where length is always 3.
*/
class buffers_triple
{
    net::mutable_buffer b_[3];

public:
    using value_type = net::mutable_buffer;
    using const_iterator = net::mutable_buffer const*;

    buffers_triple(
        buffers_triple const&) = default;

    buffers_triple& operator=(
        buffers_triple const&) = default;

    buffers_triple(char* data, std::size_t size)
    {
        b_[0] = {data, size/6};
        data += b_[0].size();
        size -= b_[0].size();
            
        b_[1] = {data, 2*size/5};
        data += b_[1].size();
        size -= b_[1].size();

        b_[2] = {data, size};

        BOOST_ASSERT(b_[0].size() > 0);
        BOOST_ASSERT(b_[1].size() > 0);
        BOOST_ASSERT(b_[2].size() > 0);
    }

    bool
    operator==(buffers_triple const& rhs) const noexcept
    {
        return
            b_[0].data() == rhs.b_[0].data() &&
            b_[0].size() == rhs.b_[0].size() &&
            b_[1].data() == rhs.b_[1].data() &&
            b_[1].size() == rhs.b_[1].size() &&
            b_[2].data() == rhs.b_[2].data() &&
            b_[2].size() == rhs.b_[2].size();
    }

    bool
    operator!=(buffers_triple const& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    const_iterator
    begin() const noexcept
    {
        return &b_[0];
    }

    const_iterator
    end() const noexcept
    {
        return &b_[3];
    }
};

template<class ConstBufferSequence>
std::size_t
buffers_length(
    ConstBufferSequence const& buffers)
{
    return std::distance(
        net::buffer_sequence_begin(buffers),
        net::buffer_sequence_end(buffers));
}

//------------------------------------------------------------------------------

namespace detail {

template<class MutableBufferSequence>
void test_mutable_buffers(
    MutableBufferSequence const&,
    net::const_buffer)
{
}

template<class MutableBufferSequence>
void test_mutable_buffers(
    MutableBufferSequence const& b,
    net::mutable_buffer)
{
    string_view src = "Hello, world!";
    BOOST_ASSERT(buffer_bytes(b) <= src.size());
    if(src.size() > buffer_bytes(b))
        src = {src.data(), buffer_bytes(b)};
    net::buffer_copy(b, net::const_buffer(
        src.data(), src.size()));
    BEAST_EXPECT(beast::buffers_to_string(b) == src);
}

} // detail

/** Test an instance of a constant or mutable buffer sequence.
*/
template<class ConstBufferSequence>
void
test_buffer_sequence(
    ConstBufferSequence const& buffers)
{
    BOOST_STATIC_ASSERT(
        net::is_const_buffer_sequence<
            ConstBufferSequence>::value);

    using iterator = decltype(
        net::buffer_sequence_begin(buffers));
    BEAST_EXPECT(sizeof(iterator) > 0);

    auto const size = buffer_bytes(buffers);
    BEAST_EXPECT(size > 0 );

    // begin, end
    auto const length = std::distance(
        net::buffer_sequence_begin(buffers),
        net::buffer_sequence_end(buffers));
    BEAST_EXPECT(length > 0);
    BEAST_EXPECT(
        net::buffer_sequence_begin(buffers) !=
        net::buffer_sequence_end(buffers));

    // copy construction
    ConstBufferSequence b1(buffers);
    BEAST_EXPECT(buffer_bytes(b1) == size);

    // copy assignment
    ConstBufferSequence b2(buffers);
    b2 = b1;
    BEAST_EXPECT(buffer_bytes(b2) == size);

    // iterators
    {
        iterator it1{};
        iterator it2{};
        iterator it3 =
            net::buffer_sequence_begin(buffers);
        iterator it4 =
            net::buffer_sequence_end(buffers);
        BEAST_EXPECT(it1 == it2);
        BEAST_EXPECT(it1 != it3);
        BEAST_EXPECT(it3 != it1);
        BEAST_EXPECT(it1 != it4);
        BEAST_EXPECT(it4 != it1);
    }

    // bidirectional
    {
        auto const first =
            net::buffer_sequence_begin(buffers);
        auto const last =
            net::buffer_sequence_end(buffers);
        std::size_t n, m;
        iterator it;

        // pre-increment
        m = 0;
        n = length;
        for(it = first; n--; ++it)
            m += buffer_bytes(*it);
        BEAST_EXPECT(it == last);
        BEAST_EXPECT(m == size);

        // post-increment
        m = 0;
        n = length;
        for(it = first; n--;)
            m += buffer_bytes(*it++);
        BEAST_EXPECT(it == last);
        BEAST_EXPECT(m == size);

        // pre-decrement
        m = 0;
        n = length;
        for(it = last; n--;)
            m += buffer_bytes(*--it);
        BEAST_EXPECT(it == first);
        BEAST_EXPECT(m == size);

        // post-decrement
        m = 0;
        n = length;
        for(it = last; n--;)
        {
            it--;
            m += buffer_bytes(*it);
        }
        BEAST_EXPECT(it == first);
        BEAST_EXPECT(m == size);
    }

    detail::test_mutable_buffers(buffers,
        buffers_type<ConstBufferSequence>{});
}

//------------------------------------------------------------------------------

/** Metafunction to determine if a type meets the requirements of MutableDynamicBuffer
*/
/* @{ */
// VFALCO This trait needs tests
template<class T, class = void>
struct is_mutable_dynamic_buffer
    : std::false_type
{
};

template<class T>
struct is_mutable_dynamic_buffer<T, detail::void_t<decltype(
    std::declval<typename T::const_buffers_type&>() =
        std::declval<T const&>().data(),
    std::declval<typename T::const_buffers_type&>() =
        std::declval<T&>().cdata(),
    std::declval<typename T::mutable_data_type&>() =
        std::declval<T&>().data()
    ) > > : net::is_dynamic_buffer<T>
{
};
/** @} */

namespace detail {

template<class MutableBufferSequence>
void
buffers_fill(
    MutableBufferSequence const& buffers,
    char c)
{
    auto const end =
        net::buffer_sequence_end(buffers);
    for(auto it = net::buffer_sequence_begin(buffers);
        it != end; ++it)
    {
        net::mutable_buffer b(*it);
        std::fill(
            static_cast<char*>(b.data()),
            static_cast<char*>(b.data()) + b.size(), c);
    }
}

template<class MutableDynamicBuffer>
void
test_mutable_dynamic_buffer(
    MutableDynamicBuffer const&,
    std::false_type)
{
}

template<class MutableDynamicBuffer>
void
test_mutable_dynamic_buffer(
    MutableDynamicBuffer const& b0,
    std::true_type)
{
    BOOST_STATIC_ASSERT(
        net::is_mutable_buffer_sequence<typename
            MutableDynamicBuffer::mutable_data_type>::value);

    BOOST_STATIC_ASSERT(
        std::is_convertible<
            typename MutableDynamicBuffer::mutable_data_type,
            typename MutableDynamicBuffer::const_buffers_type>::value);

    string_view src = "Hello, world!";
    if(src.size() > b0.max_size())
        src = {src.data(), b0.max_size()};

    // modify readable bytes
    {
        MutableDynamicBuffer b(b0);
        auto const mb = b.prepare(src.size());
        BEAST_EXPECT(buffer_bytes(mb) == src.size());
        buffers_fill(mb, '*');
        b.commit(src.size());
        BEAST_EXPECT(b.size() == src.size());
        BEAST_EXPECT(
            beast::buffers_to_string(b.data()) ==
            std::string(src.size(), '*'));
        BEAST_EXPECT(
            beast::buffers_to_string(b.cdata()) ==
            std::string(src.size(), '*'));
        auto const n = net::buffer_copy(
            b.data(), net::const_buffer(
                src.data(), src.size()));
        BEAST_EXPECT(n == src.size());
        BEAST_EXPECT(
            beast::buffers_to_string(b.data()) == src);
        BEAST_EXPECT(
            beast::buffers_to_string(b.cdata()) == src);
    }

    // mutable to const sequence conversion
    {
        MutableDynamicBuffer b(b0);
        b.commit(net::buffer_copy(
            b.prepare(src.size()),
            net::const_buffer(src.data(), src.size())));
        auto mb = b.data();
        auto cb = static_cast<
            MutableDynamicBuffer const&>(b).data();
        auto cbc = b.cdata();
        BEAST_EXPECT(
            beast::buffers_to_string(b.data()) == src);
        BEAST_EXPECT(
            beast::buffers_to_string(b.cdata()) == src);
        beast::test_buffer_sequence(cb);
        beast::test_buffer_sequence(cbc);
        beast::test_buffer_sequence(mb);
        {
            decltype(mb)  mb2(mb);
            mb = mb2;
            decltype(cb)  cb2(cb);
            cb = cb2;
            decltype(cbc) cbc2(cbc);
            cbc = cbc2;
        }
        {
            decltype(cb)  cb2(mb);
            decltype(cbc) cbc2(mb);
            cb2 = mb;
            cbc2 = mb;
        }
    }
}

} // detail

/** Test an instance of a dynamic buffer or mutable dynamic buffer.
*/
template<class DynamicBuffer>
void
test_dynamic_buffer(
    DynamicBuffer const& b0)
{
    BOOST_STATIC_ASSERT(
        net::is_dynamic_buffer<DynamicBuffer>::value);

    BOOST_STATIC_ASSERT(
        net::is_const_buffer_sequence<typename
            DynamicBuffer::const_buffers_type>::value);

    BOOST_STATIC_ASSERT(
        net::is_mutable_buffer_sequence<typename
            DynamicBuffer::mutable_buffers_type>::value);

    BEAST_EXPECT(b0.size() == 0);
    BEAST_EXPECT(buffer_bytes(b0.data()) == 0);

    // members
    {
        string_view src = "Hello, world!";

        DynamicBuffer b1(b0);
        auto const mb = b1.prepare(src.size());
        b1.commit(net::buffer_copy(mb,
            net::const_buffer(src.data(), src.size())));

        // copy constructor
        {
            DynamicBuffer b2(b1);
            BEAST_EXPECT(b2.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b1.data()) ==
                buffers_to_string(b2.data()));

            // https://github.com/boostorg/beast/issues/1621
            b2.consume(1);
            DynamicBuffer b3(b2);
            BEAST_EXPECT(b3.size() == b2.size());
            BEAST_EXPECT(
                buffers_to_string(b2.data()) ==
                buffers_to_string(b3.data()));
        }

        // move constructor
        {
            DynamicBuffer b2(b1);
            DynamicBuffer b3(std::move(b2));
            BEAST_EXPECT(b3.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b3.data()) ==
                buffers_to_string(b1.data()));
        }

        // copy assignment
        {
            DynamicBuffer b2(b0);
            b2 = b1;
            BEAST_EXPECT(b2.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b1.data()) ==
                buffers_to_string(b2.data()));

            // self assignment
            b2 = *&b2;
            BEAST_EXPECT(b2.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b2.data()) ==
                buffers_to_string(b1.data()));

            // https://github.com/boostorg/beast/issues/1621
            b2.consume(1);
            DynamicBuffer b3(b2);
            BEAST_EXPECT(b3.size() == b2.size());
            BEAST_EXPECT(
                buffers_to_string(b2.data()) ==
                buffers_to_string(b3.data()));

        }

        // move assignment
        {
            DynamicBuffer b2(b1);
            DynamicBuffer b3(b0);
            b3 = std::move(b2);
            BEAST_EXPECT(b3.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b3.data()) ==
                buffers_to_string(b1.data()));

            // self move
            b3 = std::move(b3);
            BEAST_EXPECT(b3.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b3.data()) ==
                buffers_to_string(b1.data()));
        }

        // swap
        {
            DynamicBuffer b2(b1);
            DynamicBuffer b3(b0);
            BEAST_EXPECT(b2.size() == b1.size());
            BEAST_EXPECT(b3.size() == b0.size());
            using std::swap;
            swap(b2, b3);
            BEAST_EXPECT(b2.size() == b0.size());
            BEAST_EXPECT(b3.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b3.data()) ==
                buffers_to_string(b1.data()));
        }
    }

    // n == 0
    {
        DynamicBuffer b(b0);
        b.commit(1);
        BEAST_EXPECT(b.size() == 0);
        BEAST_EXPECT(buffer_bytes(b.prepare(0)) == 0);
        b.commit(0);
        BEAST_EXPECT(b.size() == 0);
        b.commit(1);
        BEAST_EXPECT(b.size() == 0);
        b.commit(b.max_size() + 1);
        BEAST_EXPECT(b.size() == 0);
        b.consume(0);
        BEAST_EXPECT(b.size() == 0);
        b.consume(1);
        BEAST_EXPECT(b.size() == 0);
        b.consume(b.max_size() + 1);
        BEAST_EXPECT(b.size() == 0);
    }

    // max_size
    {
        DynamicBuffer b(b0);
        if(BEAST_EXPECT(
            b.max_size() + 1 > b.max_size()))
        {
            try
            {
                b.prepare(b.max_size() + 1);
                BEAST_FAIL();
            }
            catch(std::length_error const&)
            {
                BEAST_PASS();
            }
            catch(...)
            {
                BEAST_FAIL();
            }
        }
    }

    // setup source buffer
    char buf[13];
    unsigned char k0 = 0;
    string_view src(buf, sizeof(buf));
    if(src.size() > b0.max_size())
        src = {src.data(), b0.max_size()};
    BEAST_EXPECT(b0.max_size() >= src.size());
    BEAST_EXPECT(b0.size() == 0);
    BEAST_EXPECT(buffer_bytes(b0.data()) == 0);
    auto const make_new_src =
        [&buf, &k0, &src]
        {
            auto k = k0++;
            for(std::size_t i = 0; i < src.size(); ++i)
                buf[i] = k++;
        };

    // readable / writable buffer sequence tests
    {
        make_new_src();
        DynamicBuffer b(b0);
        auto const& bc(b);
        auto const mb = b.prepare(src.size());
        BEAST_EXPECT(buffer_bytes(mb) == src.size());
        beast::test_buffer_sequence(mb);
        b.commit(net::buffer_copy(mb,
            net::const_buffer(src.data(), src.size())));
        BEAST_EXPECT(
            buffer_bytes(bc.data()) == src.size());
        beast::test_buffer_sequence(bc.data());
    }

    // h = in size
    // i = prepare size
    // j = commit size
    // k = consume size
    for(std::size_t h = 1; h <= src.size(); ++h)
    {
        string_view in(src.data(), h);
        for(std::size_t i = 1; i <= in.size(); ++i) {
        for(std::size_t j = 1; j <= i + 1; ++j) {
        for(std::size_t k = 1; k <= in.size(); ++k) {
        {
            make_new_src();

            DynamicBuffer b(b0);
            auto const& bc(b);
            net::const_buffer cb(in.data(), in.size());
            while(cb.size() > 0)
            {
                auto const mb = b.prepare(
                    std::min<std::size_t>(i,
                        b.max_size() - b.size()));
                auto const n = net::buffer_copy(mb,
                    net::const_buffer(cb.data(),
                        std::min<std::size_t>(j, cb.size())));
                b.commit(n);
                cb += n;
            }
            BEAST_EXPECT(b.size() == in.size());
            BEAST_EXPECT(
                buffer_bytes(bc.data()) == in.size());
            BEAST_EXPECT(beast::buffers_to_string(
                bc.data()) == in);
            while(b.size() > 0)
                b.consume(k);
            BEAST_EXPECT(buffer_bytes(bc.data()) == 0);
        }
        } } }
    }

    // MutableDynamicBuffer refinement
    detail::test_mutable_dynamic_buffer(b0,
        is_mutable_dynamic_buffer<DynamicBuffer>{});
}

} // beast
} // boost

#endif
