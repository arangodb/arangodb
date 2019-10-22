//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <cstddef>
#include <iostream>
#include <memory>
#include <utility>

namespace net = boost::asio;
namespace beast = boost::beast;

//[example_core_echo_op_1

template<
    class AsyncStream,
    class DynamicBuffer,
    class CompletionToken>
auto
async_echo (AsyncStream& stream, DynamicBuffer& buffer, CompletionToken&& token)

//]
    ->
    typename net::async_result<
        typename std::decay<CompletionToken>::type,
        void(beast::error_code)>::return_type;

//------------------------------------------------------------------------------

//[example_core_echo_op_2

/** Asynchronously read a line and echo it back.

    This function is used to asynchronously read a line ending
    in a newline (`"\n"`) from the stream, and then write
    it back.

    This call always returns immediately. The asynchronous operation
    will continue until one of the following conditions is true:

    @li A line was read in and written back on the stream

    @li An error occurs.

    The algorithm, known as a <em>composed asynchronous operation</em>,
    is implemented in terms of calls to the stream's `async_read_some`
    and `async_write_some` function. The program must ensure that no
    other reads or writes are performed until this operation completes.

    Since the length of the line is not known ahead of time, the
    implementation may read additional characters that lie past the
    first line. These characters are stored in the dynamic buffer_.
    The same dynamic buffer must be presented again in each call,
    to provide the implementation with any leftover bytes.

    @param stream The stream to operate on. The type must meet the
    requirements of <em>AsyncReadStream</em> and @AsyncWriteStream

    @param buffer A dynamic buffer to hold implementation-defined
    temporary data. Ownership is not transferred; the caller is
    responsible for ensuring that the lifetime of this object is
    extended least until the completion handler is invoked.

    @param token The handler to be called when the operation completes.
    The implementation takes ownership of the handler by performing a decay-copy.
    The handler must be invocable with this signature:
    @code
    void handler(
        beast::error_code error      // Result of operation.
    );
    @endcode

    Regardless of whether the asynchronous operation completes immediately or
    not, the handler will not be invoked from within this function. Invocation
    of the handler will be performed in a manner equivalent to using
    `net::post`.
*/
template<
    class AsyncStream,
    class DynamicBuffer,
    class CompletionToken>
auto
async_echo (
    AsyncStream& stream,
    DynamicBuffer& buffer, /*< Unlike Asio, we pass by non-const reference instead of rvalue-ref >*/
    CompletionToken&& token) ->
        typename net::async_result< /*< `async_result` deduces the return type from the completion handler >*/
            typename std::decay<CompletionToken>::type,
            void(beast::error_code) /*< The completion handler signature goes here >*/
                >::return_type;
//]

//[example_core_echo_op_3

template<class AsyncStream, class Handler>
class echo_op;

// This example uses the Asio's stackless "fauxroutines", implemented
// using a macro-based solution. It makes the code easier to write and
// easier to read. This include file defines the necessary macros and types.
#include <boost/asio/yield.hpp>

// Read a line and echo it back
//
template<
    class AsyncStream,
    class DynamicBuffer,
    class CompletionToken>
auto
async_echo(
    AsyncStream& stream,
    DynamicBuffer& buffer,
    CompletionToken&& token) ->
        typename net::async_result<
            typename std::decay<CompletionToken>::type,
            void(beast::error_code)>::return_type /*< The completion handler signature goes here >*/
{
    // Perform some type checks using static assert, this helps
    // with more friendly error messages when passing the wrong types.
    static_assert(
        beast::is_async_stream<AsyncStream>::value,
        "AsyncStream type requirements not met");
    static_assert(
        net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer type requirements not met");

    // This class template deduces the actual handler type from a
    // CompletionToken, captures a local reference to the handler,
    // and creates the `async_result` object which becomes the
    // return value of this initiating function.

    net::async_completion<CompletionToken, void(beast::error_code)> init(token);

    // The helper macro BOOST_ASIO_HANDLER_TYPE converts the completion
    // token type into a concrete handler type of the correct signature.

    using handler_type = BOOST_ASIO_HANDLER_TYPE(CompletionToken, void(beast::error_code));

    // The class template `async_base` holds the caller's completion
    // handler for us, and provides all of the boilerplate for forwarding
    // the associated allocator and associated executor from the caller's
    // handler to our operation. It also maintains a `net::executor_work_guard`
    // for the executor associated with the stream. This work guard is
    // inexpensive, and prevents the execution context from running out
    // of work. It is usually necessary although rarely it can be skipped
    // depending on the operation (this echo example needs it because it
    // performs more than one asynchronous operation in a row).
    // We declare this type alias to make the code easier to read.

    using base_type = beast::async_base<
        handler_type, /*< The type of the completion handler obtained from the token >*/
        beast::executor_type<AsyncStream> /*< The type of executor used by the stream to dispatch asynchronous operations >*/
    >;

    // This nested class implements the echo composed operation as a
    // stateful completion handler. We derive from `async_base` to
    // take care of boilerplate and we derived from asio::coroutine to
    // allow the reenter and yield keywords to work.

    struct echo_op : base_type, boost::asio::coroutine
    {
        AsyncStream& stream_;
        DynamicBuffer& buffer_;

        echo_op(
            AsyncStream& stream,
            DynamicBuffer& buffer,
            handler_type&& handler)
            : base_type(
                std::move(handler), /*< The `async_base` helper takes ownership of the handler, >*/
                stream.get_executor()) /*<  and also needs to know which executor to use. >*/
            , stream_(stream)
            , buffer_(buffer)
        {
            // Launch the operation directly from the constructor. We
            // pass `false` for `cont` to indicate that the calling
            // thread does not represent a continuation of our
            // asynchronous control flow.
            (*this)({}, 0, false);
        }

        // If a newline is present in the buffer sequence, this function returns
        // the number of characters from the beginning of the buffer up to the
        // newline, including the newline character. Otherwise it returns zero.

        std::size_t
        find_newline(typename DynamicBuffer::const_buffers_type const& buffers)
        {
            // The `buffers_iterator` class template provides random-access
            // iterators into a buffer sequence. Use the standard algorithm
            // to look for the new line if it exists.

            auto begin = net::buffers_iterator<
                typename DynamicBuffer::const_buffers_type>::begin(buffers);
            auto end =   net::buffers_iterator<
                typename DynamicBuffer::const_buffers_type>::end(buffers);
            auto result = std::find(begin, end, '\n');

            if(result == end)
                return 0; // not found

            return result + 1 - begin;
        }

        // This is the entry point of our completion handler. Every time an
        // asynchronous operation completes, this function will be invoked.

        void
        operator()(
            beast::error_code ec,
            std::size_t bytes_transferred = 0,
            bool cont = true) /*< Second and subsequent invocations will seee `cont=true`. */
        {
            // The `reenter` keyword transfers control to the last
            // yield point, or to the beginning of the scope if
            // this is the first time.

            reenter(*this)
            {
                for(;;)
                {
                    std::size_t pos;

                    // Search for a newline in the readable bytes of the buffer
                    pos = find_newline(buffer_.data());

                    // If we don't have the newline, then read more
                    if(pos == 0)
                    {
                        std::size_t bytes_to_read;

                        // Determine the number of bytes to read,
                        // using available capacity in the buffer first.

                        bytes_to_read = std::min<std::size_t>(
                              std::max<std::size_t>(512,                // under 512 is too little,
                                  buffer_.capacity() - buffer_.size()),
                              std::min<std::size_t>(65536,              // and over 65536 is too much.
                                  buffer_.max_size() - buffer_.size()));

                        // Read some data into our dynamic buffer_. We transfer
                        // ownership of the composed operation by using the
                        // `std::move(*this)` idiom. The `yield` keyword causes
                        // the function to return immediately after the initiating
                        // function returns.

                        yield stream_.async_read_some(
                            buffer_.prepare(bytes_to_read), std::move(*this));

                        // After the `async_read_some` completes, control is
                        // transferred to this line by the `reenter` keyword.

                        // Move the bytes read from the writable area to the
                        // readable area.

                        buffer_.commit(bytes_transferred);

                        // If an error occurs, deliver it to the caller's completion handler.
                        if(ec)
                            break;

                        // Keep looping until we get the newline
                        continue;
                    }

                    // We have our newline, so send the first `pos` bytes of the
                    // buffers. The function `buffers_prefix` returns the front part
                    // of the buffers we want.

                    yield net::async_write(stream_,
                        beast::buffers_prefix(pos, buffer_.data()), std::move(*this));

                    // After the `async_write` completes, our completion handler will
                    // be invoked with the error and the number of bytes transferred,
                    // and the `reenter` statement above will cause control to jump
                    // to the following line. The variable `pos` is no longer valid
                    // (remember that we returned from the function using `yield` above)
                    // but we can use `bytes_transferred` to know how much of the buffer
                    // to consume. With "real" coroutines this will be easier and more
                    // natural.

                    buffer_.consume(bytes_transferred);

                    // The loop terminates here, and we will either deliver a
                    // successful result or an error to the caller's completion handler.

                    break;
                }

                // When a composed operation completes immediately, it must not
                // directly invoke the completion handler otherwise it could
                // lead to unfairness, starvation, or stack overflow. Therefore,
                // if cont == false (meaning, that the call stack still includes
                // the frame of the initiating function) then we need to use
                // `net::post` to cause us to be called again after the initiating
                // function. The function `async_base::invoke` takes care of
                // calling the final completion handler, using post if the
                // first argument is false, otherwise invoking it directly.

                this->complete(cont, ec);
            }
        }
    };

    // Create the composed operation and launch it. This is a constructor
    // call followed by invocation of operator(). We use BOOST_ASIO_HANDLER_TYPE
    // to convert the completion token into the correct handler type,
    // allowing user-defined specializations of the async_result template
    // to be used.

    echo_op(stream, buffer, std::move(init.completion_handler));

    // This hook lets the caller see a return value when appropriate.
    // For example this might return std::future<error_code> if
    // CompletionToken is net::use_future, or this might
    // return an error code if CompletionToken specifies a coroutine.

    return init.result.get();
}

// Including this file undefines the macros used by the stackless fauxroutines.
#include <boost/asio/unyield.hpp>

//]

struct move_only_handler
{
    move_only_handler(move_only_handler&&) = default;
    move_only_handler(move_only_handler const&) = delete;

    void operator()(beast::error_code ec)
    {
        if(ec)
            std::cerr << ": " << ec.message() << std::endl;
    }
};

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        std::cerr
        << "Usage: echo-op <address> <port>\n"
        << "Example:\n"
        << "    echo-op 0.0.0.0 8080\n";
        return EXIT_FAILURE;
    }

    namespace net = boost::asio;
    auto const address{net::ip::make_address(argv[1])};
    auto const port{static_cast<unsigned short>(std::atoi(argv[2]))};

    using endpoint_type = net::ip::tcp::endpoint;

    // Create a listening socket, accept a connection, perform
    // the echo, and then shut everything down and exit.
    net::io_context ioc;
    net::ip::tcp::acceptor acceptor{ioc};
    endpoint_type ep{address, port};
    acceptor.open(ep.protocol());
    acceptor.set_option(net::socket_base::reuse_address(true));
    acceptor.bind(ep);
    acceptor.listen();
    auto sock = acceptor.accept();
    beast::flat_buffer buffer;
    async_echo(sock, buffer, move_only_handler{});
    ioc.run();
    return EXIT_SUCCESS;
}
