//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_COMMON_DETECT_SSL_HPP
#define BOOST_BEAST_EXAMPLE_COMMON_DETECT_SSL_HPP

#include <boost/assert.hpp>
#include <boost/config.hpp>

//------------------------------------------------------------------------------
//
// Example: Detect TLS/SSL
//
//------------------------------------------------------------------------------

//[example_core_detect_ssl_1

#include <boost/beast/core.hpp>
#include <boost/logic/tribool.hpp>

/** Return `true` if a buffer contains a TLS/SSL client handshake.

    This function returns `true` if the beginning of the buffer
    indicates that a TLS handshake is being negotiated, and that
    there are at least four octets in the buffer.

    If the content of the buffer cannot possibly be a TLS handshake
    request, the function returns `false`. Otherwise, if additional
    octets are required, `boost::indeterminate` is returned.

    @param buffer The input buffer to inspect. This type must meet
    the requirements of @b ConstBufferSequence.

    @return `boost::tribool` indicating whether the buffer contains
    a TLS client handshake, does not contain a handshake, or needs
    additional octets.

    @see

    http://www.ietf.org/rfc/rfc2246.txt
    7.4. Handshake protocol
*/
template<class ConstBufferSequence>
boost::tribool
is_ssl_handshake(ConstBufferSequence const& buffers);

//]

//[example_core_detect_ssl_2

template<
    class ConstBufferSequence>
boost::tribool
is_ssl_handshake(
    ConstBufferSequence const& buffers)
{
    // Make sure buffers meets the requirements
    static_assert(
        boost::asio::is_const_buffer_sequence<ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");

    // We need at least one byte to really do anything
    if(boost::asio::buffer_size(buffers) < 1)
        return boost::indeterminate;

    // Extract the first byte, which holds the
    // "message" type for the Handshake protocol.
    unsigned char v;
    boost::asio::buffer_copy(boost::asio::buffer(&v, 1), buffers);

    // Check that the message type is "SSL Handshake" (rfc2246)
    if(v != 0x16)
    {
        // This is definitely not a handshake
        return false;
    }

    // At least four bytes are needed for the handshake
    // so make sure that we get them before returning `true`
    if(boost::asio::buffer_size(buffers) < 4)
        return boost::indeterminate;

    // This can only be a TLS/SSL handshake
    return true;
}

//]

//[example_core_detect_ssl_3

/** Detect a TLS/SSL handshake on a stream.

    This function reads from a stream to determine if a TLS/SSL
    handshake is being received. The function call will block
    until one of the following conditions is true:

    @li The disposition of the handshake is determined

    @li An error occurs

    Octets read from the stream will be stored in the passed dynamic
    buffer, which may be used to perform the TLS handshake if the
    detector returns true, or otherwise consumed by the caller based
    on the expected protocol.

    @param stream The stream to read from. This type must meet the
    requirements of @b SyncReadStream.

    @param buffer The dynamic buffer to use. This type must meet the
    requirements of @b DynamicBuffer.

    @param ec Set to the error if any occurred.

    @return `boost::tribool` indicating whether the buffer contains
    a TLS client handshake, does not contain a handshake, or needs
    additional octets. If an error occurs, the return value is
    undefined.
*/
template<
    class SyncReadStream,
    class DynamicBuffer>
boost::tribool
detect_ssl(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    boost::beast::error_code& ec)
{
    namespace beast = boost::beast;

    // Make sure arguments meet the requirements
    static_assert(beast::is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");

    // Loop until an error occurs or we get a definitive answer
    for(;;)
    {
        // There could already be data in the buffer
        // so we do this first, before reading from the stream.
        auto const result = is_ssl_handshake(buffer.data());

        // If we got an answer, return it
        if(! boost::indeterminate(result))
        {
            // This is a fast way to indicate success
            // without retrieving the default category.
            ec.assign(0, ec.category());
            return result;
        }

        // The algorithm should never need more than 4 bytes
        BOOST_ASSERT(buffer.size() < 4);

        // Prepare the buffer's output area.
        auto const mutable_buffer = buffer.prepare(beast::read_size(buffer, 1536));

        // Try to fill our buffer by reading from the stream
        std::size_t const bytes_transferred = stream.read_some(mutable_buffer, ec);

        // Check for an error
        if(ec)
            break;

        // Commit what we read into the buffer's input area.
        buffer.commit(bytes_transferred);
    }

    // error
    return false;
}

//]

//[example_core_detect_ssl_4

/** Detect a TLS/SSL handshake asynchronously on a stream.

    This function is used to asynchronously determine if a TLS/SSL
    handshake is being received.
    The function call always returns immediately. The asynchronous
    operation will continue until one of the following conditions
    is true:

    @li The disposition of the handshake is determined

    @li An error occurs

    This operation is implemented in terms of zero or more calls to
    the next layer's `async_read_some` function, and is known as a
    <em>composed operation</em>. The program must ensure that the
    stream performs no other operations until this operation completes.

    Octets read from the stream will be stored in the passed dynamic
    buffer, which may be used to perform the TLS handshake if the
    detector returns true, or otherwise consumed by the caller based
    on the expected protocol.

    @param stream The stream to read from. This type must meet the
    requirements of @b AsyncReadStream.

    @param buffer The dynamic buffer to use. This type must meet the
    requirements of @b DynamicBuffer.

    @param handler The handler to be called when the request
    completes. Copies will be made of the handler as required.
    The equivalent function signature of the handler must be:
    @code
    void handler(
        error_code const& error,    // Set to the error, if any
        boost::tribool result       // The result of the detector
    );
    @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_context::post`.
*/
template<
    class AsyncReadStream,
    class DynamicBuffer,
    class CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(                      /*< `BOOST_ASIO_INITFN_RESULT_TYPE` customizes the return value based on the completion token >*/
    CompletionToken,
    void(boost::beast::error_code, boost::tribool)) /*< This is the signature for the completion handler >*/
async_detect_ssl(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    CompletionToken&& token);

//]

//[example_core_detect_ssl_5

// This is the composed operation.
template<
    class AsyncReadStream,
    class DynamicBuffer,
    class Handler>
class detect_ssl_op;

// Here is the implementation of the asynchronous initation function
template<
    class AsyncReadStream,
    class DynamicBuffer,
    class CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    void(boost::beast::error_code, boost::tribool))
async_detect_ssl(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    CompletionToken&& token)
{
    namespace beast = boost::beast;

    // Make sure arguments meet the requirements
    static_assert(beast::is_async_read_stream<AsyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");

    // This helper manages some of the handler's lifetime and
    // uses the result and handler specializations associated with
    // the completion token to help customize the return value.
    //
    boost::asio::async_completion<
        CompletionToken, void(beast::error_code, boost::tribool)> init{token};

    // Create the composed operation and launch it. This is a constructor
    // call followed by invocation of operator(). We use BOOST_ASIO_HANDLER_TYPE
    // to convert the completion token into the correct handler type,
    // allowing user defined specializations of the async result template
    // to take effect.
    //
    detect_ssl_op<
        AsyncReadStream,
        DynamicBuffer,
        BOOST_ASIO_HANDLER_TYPE(
            CompletionToken, void(beast::error_code, boost::tribool))>{
                stream, buffer, init.completion_handler}(beast::error_code{}, 0);

    // This hook lets the caller see a return value when appropriate.
    // For example this might return std::future<error_code, boost::tribool> if
    // CompletionToken is boost::asio::use_future.
    //
    // If a coroutine is used for the token, the return value from
    // this function will be the `boost::tribool` representing the result.
    //
    return init.result.get();
}

//]

//[example_core_detect_ssl_6

// Read from a stream to invoke is_tls_handshake asynchronously
//
template<
    class AsyncReadStream,
    class DynamicBuffer,
    class Handler>
class detect_ssl_op
{
    // This composed operation has trivial state,
    // so it is just kept inside the class and can
    // be cheaply copied as needed by the implementation.

    // Indicates what step in the operation's state
    // machine to perform next, starting from zero.
    int step_ = 0;

    AsyncReadStream& stream_;
    DynamicBuffer& buffer_;
    Handler handler_;
    boost::tribool result_ = false;

public:
    // Boost.Asio requires that handlers are CopyConstructible.
    // The state for this operation is cheap to copy.
    detect_ssl_op(detect_ssl_op const&) = default;

    // The constructor just keeps references the callers varaibles.
    //
    template<class DeducedHandler>
    detect_ssl_op(
        AsyncReadStream& stream,
        DynamicBuffer& buffer,
        DeducedHandler&& handler)
        : stream_(stream)
        , buffer_(buffer)
        , handler_(std::forward<DeducedHandler>(handler))
    {
    }

    // Associated allocator support. This is Asio's system for
    // allowing the final completion handler to customize the
    // memory allocation strategy used for composed operation
    // states. A composed operation needs to use the same allocator
    // as the final handler. These declarations achieve that.

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler_);
    }

    // Executor hook. This is Asio's system for customizing the
    // manner in which asynchronous completion handlers are invoked.
    // A composed operation needs to use the same executor to invoke
    // intermediate completion handlers as that used to invoke the
    // final handler.

    using executor_type = boost::asio::associated_executor_t<
        Handler, decltype(std::declval<AsyncReadStream&>().get_executor())>;

    executor_type get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(handler_, stream_.get_executor());
    }

    // Determines if the next asynchronous operation represents a
    // continuation of the asynchronous flow of control associated
    // with the final handler. If we are past step two, it means
    // we have performed an asynchronous operation therefore any
    // subsequent operation would represent a continuation.
    // Otherwise, we propagate the handler's associated value of
    // is_continuation. Getting this right means the implementation
    // may schedule the invokation of the invoked functions more
    // efficiently.
    //
    friend bool asio_handler_is_continuation(detect_ssl_op* op)
    {
        // This next call is structured to permit argument
        // dependent lookup to take effect.
        using boost::asio::asio_handler_is_continuation;

        // Always use std::addressof to pass the pointer to the handler,
        // otherwise an unwanted overload of operator& may be called instead.
        return op->step_ > 2 ||
            asio_handler_is_continuation(std::addressof(op->handler_));
    }

    // Our main entry point. This will get called as our
    // intermediate operations complete. Definition below.
    //
    void operator()(boost::beast::error_code ec, std::size_t bytes_transferred);
};

//]

//[example_core_detect_ssl_7

// detect_ssl_op is callable with the signature
// void(error_code, bytes_transferred),
// allowing `*this` to be used as a ReadHandler
//
template<
    class AsyncStream,
    class DynamicBuffer,
    class Handler>
void
detect_ssl_op<AsyncStream, DynamicBuffer, Handler>::
operator()(boost::beast::error_code ec, std::size_t bytes_transferred)
{
    namespace beast = boost::beast;

    // Execute the state machine
    switch(step_)
    {
    // Initial state
    case 0:
        // See if we can detect the handshake
        result_ = is_ssl_handshake(buffer_.data());

        // If there's a result, call the handler
        if(! boost::indeterminate(result_))
        {
            // We need to invoke the handler, but the guarantee
            // is that the handler will not be called before the
            // call to async_detect_ssl returns, so we must post
            // the operation to the executor. The helper function
            // `bind_handler` lets us bind arguments in a safe way
            // that preserves the type customization hooks of the
            // original handler.
            step_ = 1;
            return boost::asio::post(
                stream_.get_executor(),
                beast::bind_handler(std::move(*this), ec, 0));
        }

        // The algorithm should never need more than 4 bytes
        BOOST_ASSERT(buffer_.size() < 4);

        step_ = 2;

    do_read:
        // We need more bytes, but no more than four total.
        return stream_.async_read_some(buffer_.prepare(beast::read_size(buffer_, 1536)), std::move(*this));

    case 1:
        // Call the handler
        break;

    case 2:
        // Set this so that asio_handler_is_continuation knows that
        // the next asynchronous operation represents a continuation
        // of the initial asynchronous operation.
        step_ = 3;
        BOOST_FALLTHROUGH;

    case 3:
        if(ec)
        {
            // Deliver the error to the handler
            result_ = false;

            // We don't need bind_handler here because we were invoked
            // as a result of an intermediate asynchronous operation.
            break;
        }

        // Commit the bytes that we read
        buffer_.commit(bytes_transferred);

        // See if we can detect the handshake
        result_ = is_ssl_handshake(buffer_.data());

        // If it is detected, call the handler
        if(! boost::indeterminate(result_))
        {
            // We don't need bind_handler here because we were invoked
            // as a result of an intermediate asynchronous operation.
            break;
        }

        // Read some more
        goto do_read;
    }

    // Invoke the final handler.
    handler_(ec, result_);
}

//]

#endif
