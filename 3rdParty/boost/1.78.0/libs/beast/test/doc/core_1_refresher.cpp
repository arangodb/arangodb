//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "snippets.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <algorithm>
#include <assert.h>

namespace boost {
namespace beast {

namespace {

void
snippets()
{
    #include "snippets.ipp"
    {
    //[code_core_1_refresher_1s
        net::const_buffer cb("Hello, world!", 13);
        assert(string_view(reinterpret_cast<char const*>(
            cb.data()), cb.size()) == "Hello, world!");

        char storage[13];
        net::mutable_buffer mb(storage, sizeof(storage));
        std::memcpy(mb.data(), cb.data(), mb.size());
        assert(string_view(reinterpret_cast<char const*>(
            mb.data()), mb.size()) == "Hello, world!");
    //]
    }
    {
    //[code_core_1_refresher_2s
        net::const_buffer b1;                   // a ConstBufferSequence by definition
        net::mutable_buffer b2;                 // a MutableBufferSequence by definition
        std::array<net::const_buffer, 3> b3;    // A ConstBufferSequence by named requirements
    //]
    }
    {
    //[code_core_1_refresher_3s
        // initiate an asynchronous write operation
        net::async_write(sock, net::const_buffer("Hello, world!", 13),
            [](error_code ec, std::size_t bytes_transferred)
            {
                // this lambda is invoked when the write operation completes
                if(! ec)
                    assert(bytes_transferred == 13);
                else
                    std::cerr << "Error: " << ec.message() << "\n";
            });
        // meanwhile, the operation is outstanding and execution continues from here
    //]
    }
    {
    //[code_core_1_refresher_4s
        std::future<std::size_t> f = net::async_write(sock,
            net::const_buffer("Hello, world!", 13), net::use_future);
    //]
    }
    {
    //[code_core_1_refresher_5s
        asio::spawn(
            [&sock](net::yield_context yield)
            {
                std::size_t bytes_transferred = net::async_write(sock,
                    net::const_buffer("Hello, world!", 13), yield);
                (void)bytes_transferred;
            });
    //]
    }
}

//------------------------------------------------------------------------------

//[code_core_1_refresher_1
template <class ConstBufferSequence>
std::string string_from_buffers (ConstBufferSequence const& buffers)
{
    // check that the type meets the requirements using the provided type traits
    static_assert(
        net::is_const_buffer_sequence<ConstBufferSequence>::value,
        "ConstBufferSequence type requirements not met");

    // optimization: reserve all the space for the string first
    std::string result;
    result.reserve(beast::buffer_bytes(buffers));        // beast version of net::buffer_size

    // iterate over each buffer in the sequence and append it to the string
    for(auto it = net::buffer_sequence_begin(buffers);  // returns an iterator to beginning of the sequence
        it != net::buffer_sequence_end(buffers);)       // returns a past-the-end iterator to the sequence
    {
        // A buffer sequence iterator's value_type is always convertible to net::const_buffer
        net::const_buffer buffer = *it++;

        // A cast is always required to out-out of type-safety
        result.append(static_cast<char const*>(buffer.data()), buffer.size());
    }
    return result;
}
//]

//------------------------------------------------------------------------------

//[code_core_1_refresher_2
// Read a line ending in '\n' from a socket, returning
// the number of characters up to but not including the newline
template <class DynamicBuffer>
std::size_t read_line(net::ip::tcp::socket& sock, DynamicBuffer& buffer)
{
    // this alias keeps things readable
    using range = net::buffers_iterator<
        typename DynamicBuffer::const_buffers_type>;

    for(;;)
    {
        // get iterators representing the range of characters in the buffer
        auto begin = range::begin(buffer.data());
        auto end = range::end(buffer.data());

        // search for "\n" and return if found
        auto pos = std::find(begin, end, '\n');
        if(pos != range::end(buffer.data()))
            return std::distance(begin, end);

        // Determine the number of bytes to read,
        // using available capacity in the buffer first.
        std::size_t bytes_to_read = std::min<std::size_t>(
              std::max<std::size_t>(512,                // under 512 is too little,
                  buffer.capacity() - buffer.size()),
              std::min<std::size_t>(65536,              // and over 65536 is too much.
                  buffer.max_size() - buffer.size()));

        // Read up to bytes_to_read bytes into the dynamic buffer
        buffer.commit(sock.read_some(buffer.prepare(bytes_to_read)));
    }
}
//]

//------------------------------------------------------------------------------

//[code_core_1_refresher_3
// Meets the requirements of SyncReadStream
struct sync_read_stream
{
    // Returns the number of bytes read upon success, otherwise throws an exception
    template <class MutableBufferSequence>
    std::size_t read_some(MutableBufferSequence const& buffers);

    // Returns the number of bytes read successfully, sets the error code if a failure occurs
    template <class MutableBufferSequence>
    std::size_t read_some(MutableBufferSequence const& buffers, error_code& ec);
};

// Meets the requirements of SyncWriteStream
struct sync_write_stream
{
    // Returns the number of bytes written upon success, otherwise throws an exception
    template <class ConstBufferSequence>
    std::size_t write_some(ConstBufferSequence const& buffers);

    // Returns the number of bytes written successfully, sets the error code if a failure occurs
    template <class ConstBufferSequence>
    std::size_t write_some(ConstBufferSequence const& buffers, error_code& ec);
};
//]

template<class MutableBufferSequence>
std::size_t sync_read_stream::read_some(MutableBufferSequence const&)
{
    return 0;
}
template<class MutableBufferSequence>
std::size_t sync_read_stream::read_some(MutableBufferSequence const&, error_code&)
{
    return 0;
}
template<class ConstBufferSequence>
std::size_t sync_write_stream::write_some(ConstBufferSequence const&)
{
    return 0;
}
template<class ConstBufferSequence>
std::size_t sync_write_stream::write_some(ConstBufferSequence const&, error_code&)
{
    return 0;
}
BOOST_STATIC_ASSERT(is_sync_read_stream<sync_read_stream>::value);
BOOST_STATIC_ASSERT(is_sync_write_stream<sync_write_stream>::value);

//------------------------------------------------------------------------------

//[code_core_1_refresher_4
template <class SyncWriteStream>
void hello (SyncWriteStream& stream)
{
    net::const_buffer cb("Hello, world!", 13);
    do
    {
        auto bytes_transferred = stream.write_some(cb); // may throw
        cb += bytes_transferred; // adjust the pointer and size
    }
    while (cb.size() > 0);
}
//]

//------------------------------------------------------------------------------

//[code_core_1_refresher_5
template <class SyncWriteStream>
void hello (SyncWriteStream& stream, error_code& ec)
{
    net::const_buffer cb("Hello, world!", 13);
    do
    {
        auto bytes_transferred = stream.write_some(cb, ec);
        cb += bytes_transferred; // adjust the pointer and size
    }
    while (cb.size() > 0 && ! ec);
}
//]

//------------------------------------------------------------------------------

} // (anon)
} // beast
} // boost

//[code_core_1_refresher_6
// The following is a completion handler expressed
// as a function object, with a nested associated
// allocator and a nested associated executor.
struct handler
{
    using allocator_type = std::allocator<char>;
    allocator_type get_allocator() const noexcept;

    using executor_type = boost::asio::io_context::executor_type;
    executor_type get_executor() const noexcept;

    void operator()(boost::beast::error_code, std::size_t);
};
//]
inline auto handler::get_allocator() const noexcept ->
    allocator_type
{
    return {};
}
inline auto handler::get_executor() const noexcept ->
    executor_type
{
    static boost::asio::io_context ioc;
    return ioc.get_executor();
}
inline void handler::operator()(
    boost::beast::error_code, std::size_t)
{
}

//[code_core_1_refresher_7
namespace boost {
namespace asio {

template<class Allocator>
struct associated_allocator<handler, Allocator>
{
    using type = std::allocator<void>;

    static
    type
    get(handler const& h,
        Allocator const& alloc = Allocator{}) noexcept;
};

template<class Executor>
struct associated_executor<handler, Executor>
{
    using type = any_io_executor;

    static
    type
    get(handler const& h,
        Executor const& ex = Executor{}) noexcept;
};

} // boost
} // asio
//]

template<class Allocator>
auto
boost::asio::associated_allocator<handler, Allocator>::
get(handler const&, Allocator const&) noexcept -> type
{
    return {};
}
template<class Executor>
auto
boost::asio::associated_executor<handler, Executor>::
get(handler const&, Executor const&) noexcept -> type
{
    return {};
}

//------------------------------------------------------------------------------

namespace boost {
namespace beast {

namespace {

//------------------------------------------------------------------------------

//[code_core_1_refresher_8
template <class AsyncWriteStream, class WriteHandler>
void async_hello (AsyncWriteStream& stream, WriteHandler&& handler)
{
    net::async_write (stream,
        net::buffer("Hello, world!", 13),
        std::forward<WriteHandler>(handler));
}
//]

//------------------------------------------------------------------------------

//[code_core_1_refresher_9
template<
    class AsyncWriteStream,
    class ConstBufferSequence,
    class CompletionToken>
auto
async_write(
    AsyncWriteStream* stream,                       // references are passed as pointers
    ConstBufferSequence const& buffers,
    CompletionToken&& token)                        // a handler, or a special object.
    ->
    typename net::async_result<                     // return-type customization point.
        typename std::decay<CompletionToken>::type, // type used to specialize async_result.
        void(error_code, std::size_t)               // underlying completion handler signature.
            >::return_type;
//]
struct run_async_write
{
    template<class... Args>
    void
    operator()(Args&&...)
    {
    }
};
template<
    class AsyncWriteStream,
    class ConstBufferSequence,
    class CompletionToken>
auto
async_write(
    AsyncWriteStream& stream,
    ConstBufferSequence const& buffers,
    CompletionToken&& token) ->
        typename net::async_result<
            typename std::decay<CompletionToken>::type,
            void(error_code, std::size_t)
                >::return_type
{
//[code_core_1_refresher_10

    return net::async_initiate<
        CompletionToken,
        void(error_code, std::size_t)>(
            run_async_write{},              // The "initiation" object.
            token,                          // Token must come before other arguments.
            &stream,                        // Additional captured arguments are
            buffers);                       //   forwarded to the initiation object.

//]
}

//------------------------------------------------------------------------------

} // (anon)

struct core_1_refresher_test
    : public beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);

        BEAST_EXPECT((static_cast<
            std::string(*)(net::const_buffer const&)>(
                &string_from_buffers<net::const_buffer>)));

        BEAST_EXPECT(static_cast<
            std::size_t(*)(net::ip::tcp::socket&, flat_buffer&)>(
                &read_line<flat_buffer>));

        BEAST_EXPECT(static_cast<
            std::size_t(sync_read_stream::*)(
                net::mutable_buffer const&)>(
                    &sync_read_stream::read_some));
        BEAST_EXPECT(static_cast<
            std::size_t(sync_read_stream::*)(
                net::mutable_buffer const&, error_code&)>(
                    &sync_read_stream::read_some));
        BEAST_EXPECT(static_cast<
            std::size_t(sync_write_stream::*)(
                net::const_buffer const&)>(
                    &sync_write_stream::write_some));
        BEAST_EXPECT(static_cast<
            std::size_t(sync_write_stream::*)(
                net::const_buffer const&, error_code&)>(
                    &sync_write_stream::write_some));

        BEAST_EXPECT(static_cast<
            void(*)(test::stream&)>(
            &hello<test::stream>));

        BEAST_EXPECT(static_cast<
            void(*)(test::stream&, error_code&)>(
            &hello<test::stream>));

        handler h;
        h.get_allocator();
        h.get_executor();

        BEAST_EXPECT((&async_hello<test::stream, handler>));
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,core_1_refresher);

} // beast
} // boost
