//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffer_traits.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/detail/is_invocable.hpp>
#include <array>

namespace boost {
namespace beast {

namespace {

struct sequence
{
    struct value_type
    {
        operator net::const_buffer() const noexcept
        {
            return {"Hello, world!", 13};
        }
    };

    using const_iterator = value_type const*;

    const_iterator begin() const noexcept
    {
        return &v_;
    }

    const_iterator end() const noexcept
    {
        return begin() + 1;
    }

private:
    value_type v_;
};

struct not_sequence
{
};

} // (anon)

class buffer_traits_test : public beast::unit_test::suite
{
public:
    // is_const_buffer_sequence

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer, net::const_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer, net::mutable_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::mutable_buffer, net::mutable_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer const&
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer const&, net::const_buffer const&
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer const&, net::mutable_buffer const&
            >::value);

    // is_mutable_buffer_sequence

    BOOST_STATIC_ASSERT(is_mutable_buffer_sequence<
            >::value);

    BOOST_STATIC_ASSERT(is_mutable_buffer_sequence<
        net::mutable_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_mutable_buffer_sequence<
        net::mutable_buffer, net::mutable_buffer
            >::value);

    BOOST_STATIC_ASSERT(! is_mutable_buffer_sequence<
        net::const_buffer, net::const_buffer
            >::value);

    BOOST_STATIC_ASSERT(! is_mutable_buffer_sequence<
        net::const_buffer, net::mutable_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_mutable_buffer_sequence<
        net::mutable_buffer const&
            >::value);

    BOOST_STATIC_ASSERT(is_mutable_buffer_sequence<
        net::mutable_buffer const&, net::mutable_buffer const&
            >::value);

    // buffers_type

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            net::const_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            net::const_buffer, net::const_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            net::const_buffer, net::mutable_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer, buffers_type<
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer, buffers_type<
            net::mutable_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer, buffers_type<
            net::mutable_buffer, net::mutable_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            std::array<net::const_buffer, 3>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer, buffers_type<
            std::array<net::mutable_buffer, 3>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            std::array<int, 3>
        >>::value);

    // buffers_iterator_type

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer const*, buffers_iterator_type<
            net::const_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer const*, buffers_iterator_type<
            net::mutable_buffer
        >>::value);

    // javadoc: buffers_type
    template <class BufferSequence>
    buffers_type <BufferSequence>
    buffers_front (BufferSequence const& buffers)
    {
        static_assert(
            net::is_const_buffer_sequence<BufferSequence>::value,
            "BufferSequence type requirements not met");
        auto const first = net::buffer_sequence_begin (buffers);
        if (first == net::buffer_sequence_end (buffers))
            return {};
        return *first;
    }

    void
    testJavadocs()
    {
        // buffers_front
        {
            net::const_buffer cb;
            buffers_front(cb);

            net::mutable_buffer mb;
            buffers_front(mb);
        }

        pass();
    }

    void
    testFunction()
    {
        BEAST_EXPECT(buffer_bytes(
            net::const_buffer("Hello, world!", 13)) == 13);

        BEAST_EXPECT(buffer_bytes(
            net::mutable_buffer{}) == 0);

        {
            sequence s;
            BEAST_EXPECT(buffer_bytes(s) == 13);
        }

        {
            std::array<net::const_buffer, 2> s({{
                net::const_buffer("Hello, world!", 13),
                net::const_buffer("Hello, world!", 13)}});
            BEAST_EXPECT(buffer_bytes(s) == 26);
        }

        BOOST_STATIC_ASSERT(! detail::is_invocable<
            detail::buffer_bytes_impl,
            std::size_t(not_sequence const&)>::value);
    }

    void run() override
    {
        testJavadocs();
        testFunction();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffer_traits);

} // beast
} // boost
