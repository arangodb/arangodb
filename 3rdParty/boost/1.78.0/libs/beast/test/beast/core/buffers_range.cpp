//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_range.hpp>

#include "test_buffer.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {

class buffers_range_test : public beast::unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        is_const_buffer_sequence<
            decltype(beast::buffers_range(
                std::declval<net::const_buffer>()))>::value);

    BOOST_STATIC_ASSERT(
        is_const_buffer_sequence<
            decltype(beast::buffers_range(
                std::declval<net::mutable_buffer>()))>::value);

    BOOST_STATIC_ASSERT(
        ! net::is_mutable_buffer_sequence<
            decltype(beast::buffers_range(
                std::declval<net::const_buffer>()))>::value);

    BOOST_STATIC_ASSERT(
        net::is_mutable_buffer_sequence<
            decltype(beast::buffers_range(
                std::declval<net::mutable_buffer>()))>::value);

    template <class BufferSequence>
    std::size_t buffer_sequence_size (BufferSequence const& buffers)
    {
        std::size_t size = 0;
        for (auto const buffer : buffers_range (buffers))
            size += buffer.size();
        return size;
    }

    template <class BufferSequence>
    std::size_t buffer_sequence_size_ref (BufferSequence const& buffers)
    {
        std::size_t size = 0;
        for (auto const buffer : buffers_range_ref (buffers))
            size += buffer.size();
        return size;
    }

    void
    testJavadocs()
    {
        BEAST_EXPECT(&buffers_range_test::buffer_sequence_size<net::const_buffer>);
        BEAST_EXPECT(&buffers_range_test::buffer_sequence_size_ref<net::const_buffer>);
    }

    void
    testBufferSequence()
    {
        {
            string_view s = "Hello, world!";
            test_buffer_sequence(buffers_range(
                net::const_buffer{s.data(), s.size()}));
        }
        {
            char buf[13];
            test_buffer_sequence(
                buffers_range(net::mutable_buffer{
                    buf, sizeof(buf)}));
        }
    }

    void
    run() override
    {
        testJavadocs();
        testBufferSequence();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_range);

} // beast
} // boost
