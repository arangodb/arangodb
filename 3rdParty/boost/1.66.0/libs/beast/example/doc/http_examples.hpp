//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast.hpp>
#include <iostream>

/*  This file contains the functions and classes found in the documentation

    They are compiled and run as part of the unit tests, so you can copy
    the code and use it in your own projects as a starting point for
    building a network application.
*/

// The documentation assumes the boost::beast::http namespace
namespace boost {
namespace beast {
namespace http {

//------------------------------------------------------------------------------
//
// Example: Expect 100-continue
//
//------------------------------------------------------------------------------

//[example_http_send_expect_100_continue

/** Send a request with Expect: 100-continue

    This function will send a request with the Expect: 100-continue
    field by first sending the header, then waiting for a successful
    response from the server before continuing to send the body. If
    a non-successful server response is received, the function
    returns immediately.

    @param stream The remote HTTP server stream.

    @param buffer The buffer used for reading.

    @param req The request to send. This function modifies the object:
    the Expect header field is inserted into the message if it does
    not already exist, and set to "100-continue".

    @param ec Set to the error, if any occurred.
*/
template<
    class SyncStream,
    class DynamicBuffer,
    class Body, class Allocator>
void
send_expect_100_continue(
    SyncStream& stream,
    DynamicBuffer& buffer,
    request<Body, basic_fields<Allocator>>& req,
    error_code& ec)
{
    static_assert(is_sync_stream<SyncStream>::value,
        "SyncStream requirements not met");

    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");

    // Insert or replace the Expect field
    req.set(field::expect, "100-continue");

    // Create the serializer
    request_serializer<Body, basic_fields<Allocator>> sr{req};

    // Send just the header
    write_header(stream, sr, ec);
    if(ec)
        return;

    // Read the response from the server.
    // A robust client could set a timeout here.
    {
        response<string_body> res;
        read(stream, buffer, res, ec);
        if(ec)
            return;
        if(res.result() != status::continue_)
        {
            // The server indicated that it will not
            // accept the request, so skip sending the body.
            return;
        }
    }

    // Server is OK with the request, send the body
    write(stream, sr, ec);
}

//]

//[example_http_receive_expect_100_continue

/** Receive a request, handling Expect: 100-continue if present.

    This function will read a request from the specified stream.
    If the request contains the Expect: 100-continue field, a
    status response will be delivered.

    @param stream The remote HTTP client stream.

    @param buffer The buffer used for reading.

    @param ec Set to the error, if any occurred.
*/
template<
    class SyncStream,
    class DynamicBuffer>
void
receive_expect_100_continue(
    SyncStream& stream,
    DynamicBuffer& buffer,
    error_code& ec)
{
    static_assert(is_sync_stream<SyncStream>::value,
        "SyncStream requirements not met");

    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");

    // Declare a parser for a request with a string body
    request_parser<string_body> parser;

    // Read the header
    read_header(stream, buffer, parser, ec);
    if(ec)
        return;

    // Check for the Expect field value
    if(parser.get()[field::expect] == "100-continue")
    {
        // send 100 response
        response<empty_body> res;
        res.version(11);
        res.result(status::continue_);
        res.set(field::server, "test");
        write(stream, res, ec);
        if(ec)
            return;
    }

    // Read the rest of the message.
    //
    // We use parser.base() to return a basic_parser&, to avoid an
    // ambiguous function error (from boost::asio::read). Another
    // solution is to qualify the call, e.g. `beast::http::read`
    //
    read(stream, buffer, parser.base(), ec);
}

//]

//------------------------------------------------------------------------------
//
// Example: Send Child Process Output
//
//------------------------------------------------------------------------------

//[example_http_send_cgi_response

/** Send the output of a child process as an HTTP response.

    The output of the child process comes from a @b SyncReadStream. Data
    will be sent continuously as it is produced, without the requirement
    that the entire process output is buffered before being sent. The
    response will use the chunked transfer encoding.

    @param input A stream to read the child process output from.

    @param output A stream to write the HTTP response to.

    @param ec Set to the error, if any occurred.
*/
template<
    class SyncReadStream,
    class SyncWriteStream>
void
send_cgi_response(
    SyncReadStream& input,
    SyncWriteStream& output,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");

    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");

    // Set up the response. We use the buffer_body type,
    // allowing serialization to use manually provided buffers.
    response<buffer_body> res;

    res.result(status::ok);
    res.version(11);
    res.set(field::server, "Beast");
    res.set(field::transfer_encoding, "chunked");

    // No data yet, but we set more = true to indicate
    // that it might be coming later. Otherwise the
    // serializer::is_done would return true right after
    // sending the header.
    res.body().data = nullptr;
    res.body().more = true;

    // Create the serializer.
    response_serializer<buffer_body, fields> sr{res};

    // Send the header immediately.
    write_header(output, sr, ec);
    if(ec)
        return;

    // Alternate between reading from the child process
    // and sending all the process output until there
    // is no more output.
    do
    {
        // Read a buffer from the child process
        char buffer[2048];
        auto bytes_transferred = input.read_some(
            boost::asio::buffer(buffer, sizeof(buffer)), ec);
        if(ec == boost::asio::error::eof)
        {
            ec = {};

            // `nullptr` indicates there is no buffer
            res.body().data = nullptr;

            // `false` means no more data is coming
            res.body().more = false;
        }
        else
        {
            if(ec)
                return;

            // Point to our buffer with the bytes that
            // we received, and indicate that there may
            // be some more data coming
            res.body().data = buffer;
            res.body().size = bytes_transferred;
            res.body().more = true;
        }
            
        // Write everything in the body buffer
        write(output, sr, ec);

        // This error is returned by body_buffer during
        // serialization when it is done sending the data
        // provided and needs another buffer.
        if(ec == error::need_buffer)
        {
            ec = {};
            continue;
        }
        if(ec)
            return;
    }
    while(! sr.is_done());
}

//]

//--------------------------------------------------------------------------
//
// Example: HEAD Request
//
//--------------------------------------------------------------------------

//[example_http_do_head_response

/** Handle a HEAD request for a resource.
*/
template<
    class SyncStream,
    class DynamicBuffer
>
void do_server_head(
    SyncStream& stream,
    DynamicBuffer& buffer,
    error_code& ec)
{
    static_assert(is_sync_stream<SyncStream>::value,
        "SyncStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirments not met");

    // We deliver this payload for all GET requests
    static std::string const payload = "Hello, world!";

    // Read the request
    request<string_body> req;
    read(stream, buffer, req, ec);
    if(ec)
        return;

    // Set up the response, starting with the common fields
    response<string_body> res;
    res.version(11);
    res.set(field::server, "test");

    // Now handle request-specific fields
    switch(req.method())
    {
    case verb::head:
    case verb::get:
    {
        // A HEAD request is handled by delivering the same
        // set of headers that would be sent for a GET request,
        // including the Content-Length, except for the body.
        res.result(status::ok);
        res.set(field::content_length, payload.size());

        // For GET requests, we include the body
        if(req.method() == verb::get)
        {
            // We deliver the same payload for GET requests
            // regardless of the target. A real server might
            // deliver a file based on the target.
            res.body() = payload;
        }
        break;
    }

    default:
    {
        // We return responses indicating an error if
        // we do not recognize the request method.
        res.result(status::bad_request);
        res.set(field::content_type, "text/plain");
        res.body() = "Invalid request-method '" + req.method_string().to_string() + "'";
        res.prepare_payload();
        break;
    }
    }

    // Send the response
    write(stream, res, ec);
    if(ec)
        return;
}

//]

//[example_http_do_head_request

/** Send a HEAD request for a resource.

    This function submits a HEAD request for the specified resource
    and returns the response.

    @param res The response. This is an output parameter.

    @param stream The synchronous stream to use.

    @param buffer The buffer to use.

    @param target The request target.

    @param ec Set to the error, if any occurred.

    @throws std::invalid_argument if target is empty.
*/
template<
    class SyncStream,
    class DynamicBuffer
>
response<empty_body>
do_head_request(
    SyncStream& stream,
    DynamicBuffer& buffer,
    string_view target,
    error_code& ec)
{
    // Do some type checking to be a good citizen
    static_assert(is_sync_stream<SyncStream>::value,
        "SyncStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirments not met");

    // The interfaces we are using are low level and do not
    // perform any checking of arguments; so we do it here.
    if(target.empty())
        throw std::invalid_argument("target may not be empty");

    // Build the HEAD request for the target
    request<empty_body> req;
    req.version(11);
    req.method(verb::head);
    req.target(target);
    req.set(field::user_agent, "test");

    // A client MUST send a Host header field in all HTTP/1.1 request messages.
    // https://tools.ietf.org/html/rfc7230#section-5.4
    req.set(field::host, "localhost");

    // Now send it
    write(stream, req, ec);
    if(ec)
        return {};

    // Create a parser to read the response.
    // We use the `empty_body` type since
    // a response to a HEAD request MUST NOT
    // include a body.
    response_parser<empty_body> p;

    // Inform the parser that there will be no body.
    p.skip(true);

    // Read the message. Even though fields like
    // Content-Length or Transfer-Encoding may be
    // set, the message will not contain a body.
    read(stream, buffer, p, ec);
    if(ec)
        return {};

    // Transfer ownership of the response to the caller.
    return p.release();
}

//]

//------------------------------------------------------------------------------
//
// Example: HTTP Relay
//
//------------------------------------------------------------------------------

//[example_http_relay

/** Relay an HTTP message.

    This function efficiently relays an HTTP message from a downstream
    client to an upstream server, or from an upstream server to a
    downstream client. After the message header is read from the input,
    a user provided transformation function is invoked which may change
    the contents of the header before forwarding to the output. This may
    be used to adjust fields such as Server, or proxy fields.

    @param output The stream to write to.

    @param input The stream to read from.

    @param buffer The buffer to use for the input.

    @param transform The header transformation to apply. The function will
    be called with this signature:
    @code
        template<class Body>
        void transform(message<
            isRequest, Body, Fields>&,  // The message to transform
            error_code&);               // Set to the error, if any
    @endcode

    @param ec Set to the error if any occurred.

    @tparam isRequest `true` to relay a request.

    @tparam Fields The type of fields to use for the message.
*/
template<
    bool isRequest,
    class SyncWriteStream,
    class SyncReadStream,
    class DynamicBuffer,
    class Transform>
void
relay(
    SyncWriteStream& output,
    SyncReadStream& input,
    DynamicBuffer& buffer,
    error_code& ec,
    Transform&& transform)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");

    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");

    // A small buffer for relaying the body piece by piece
    char buf[2048];

    // Create a parser with a buffer body to read from the input.
    parser<isRequest, buffer_body> p;

    // Create a serializer from the message contained in the parser.
    serializer<isRequest, buffer_body, fields> sr{p.get()};

    // Read just the header from the input
    read_header(input, buffer, p, ec);
    if(ec)
        return;

    // Apply the caller's header tranformation
    transform(p.get(), ec);
    if(ec)
        return;

    // Send the transformed message to the output
    write_header(output, sr, ec);
    if(ec)
        return;

    // Loop over the input and transfer it to the output
    do
    {
        if(! p.is_done())
        {
            // Set up the body for writing into our small buffer
            p.get().body().data = buf;
            p.get().body().size = sizeof(buf);

            // Read as much as we can
            read(input, buffer, p, ec);

            // This error is returned when buffer_body uses up the buffer
            if(ec == error::need_buffer)
                ec = {};
            if(ec)
                return;

            // Set up the body for reading.
            // This is how much was parsed:
            p.get().body().size = sizeof(buf) - p.get().body().size;
            p.get().body().data = buf;
            p.get().body().more = ! p.is_done();
        }
        else
        {
            p.get().body().data = nullptr;
            p.get().body().size = 0;
        }

        // Write everything in the buffer (which might be empty)
        write(output, sr, ec);

        // This error is returned when buffer_body uses up the buffer
        if(ec == error::need_buffer)
            ec = {};
        if(ec)
            return;
    }
    while(! p.is_done() && ! sr.is_done());
}

//]

//------------------------------------------------------------------------------
//
// Example: Serialize to std::ostream
//
//------------------------------------------------------------------------------

//[example_http_write_ostream

// The detail namespace means "not public"
namespace detail {

// This helper is needed for C++11.
// When invoked with a buffer sequence, writes the buffers `to the std::ostream`.
template<class Serializer>
class write_ostream_helper
{
    Serializer& sr_;
    std::ostream& os_;

public:
    write_ostream_helper(Serializer& sr, std::ostream& os)
        : sr_(sr)
        , os_(os)
    {
    }

    // This function is called by the serializer
    template<class ConstBufferSequence>
    void
    operator()(error_code& ec, ConstBufferSequence const& buffers) const
    {
        // Error codes must be cleared on success
        ec = {};

        // Keep a running total of how much we wrote
        std::size_t bytes_transferred = 0;

        // Loop over the buffer sequence
        for(auto it = boost::asio::buffer_sequence_begin(buffers);
            it != boost::asio::buffer_sequence_end(buffers); ++it)
        {
            // This is the next buffer in the sequence
            boost::asio::const_buffer const buffer = *it;

            // Write it to the std::ostream
            os_.write(
                reinterpret_cast<char const*>(buffer.data()),
                buffer.size());

            // If the std::ostream fails, convert it to an error code
            if(os_.fail())
            {
                ec = make_error_code(errc::io_error);
                return;
            }

            // Adjust our running total
            bytes_transferred += buffer_size(buffer);
        }

        // Inform the serializer of the amount we consumed
        sr_.consume(bytes_transferred);
    }
};

} // detail

/** Write a message to a `std::ostream`.

    This function writes the serialized representation of the
    HTTP/1 message to the sream.

    @param os The `std::ostream` to write to.

    @param msg The message to serialize.

    @param ec Set to the error, if any occurred.
*/
template<
    bool isRequest,
    class Body,
    class Fields>
void
write_ostream(
    std::ostream& os,
    message<isRequest, Body, Fields>& msg,
    error_code& ec)
{
    // Create the serializer instance
    serializer<isRequest, Body, Fields> sr{msg};

    // This lambda is used as the "visit" function
    detail::write_ostream_helper<decltype(sr)> lambda{sr, os};
    do
    {
        // In C++14 we could use a generic lambda but since we want
        // to require only C++11, the lambda is written out by hand.
        // This function call retrieves the next serialized buffers.
        sr.next(ec, lambda);
        if(ec)
            return;
    }
    while(! sr.is_done());
}

//]

//------------------------------------------------------------------------------
//
// Example: Parse from std::istream
//
//------------------------------------------------------------------------------

//[example_http_read_istream

/** Read a message from a `std::istream`.

    This function attempts to parse a complete HTTP/1 message from the stream.

    @param is The `std::istream` to read from.

    @param buffer The buffer to use.

    @param msg The message to store the result.

    @param ec Set to the error, if any occurred.
*/
template<
    class Allocator,
    bool isRequest,
    class Body>
void
read_istream(
    std::istream& is,
    basic_flat_buffer<Allocator>& buffer,
    message<isRequest, Body, fields>& msg,
    error_code& ec)
{
    // Create the message parser
    //
    // Arguments passed to the parser's constructor are
    // forwarded to the message constructor. Here, we use
    // a move construction in case the caller has constructed
    // their message in a non-default way.
    //
    parser<isRequest, Body> p{std::move(msg)};

    do
    {
        // Extract whatever characters are presently available in the istream
        if(is.rdbuf()->in_avail() > 0)
        {
            // Get a mutable buffer sequence for writing
            auto const b = buffer.prepare(
                static_cast<std::size_t>(is.rdbuf()->in_avail()));

            // Now get everything we can from the istream
            buffer.commit(static_cast<std::size_t>(is.readsome(
                reinterpret_cast<char*>(b.data()), b.size())));
        }
        else if(buffer.size() == 0)
        {
            // Our buffer is empty and we need more characters, 
            // see if we've reached the end of file on the istream
            if(! is.eof())
            {
                // Get a mutable buffer sequence for writing
                auto const b = buffer.prepare(1024);

                // Try to get more from the istream. This might block.
                is.read(reinterpret_cast<char*>(b.data()), b.size());

                // If an error occurs on the istream then return it to the caller.
                if(is.fail() && ! is.eof())
                {
                    // We'll just re-use io_error since std::istream has no error_code interface.
                    ec = make_error_code(errc::io_error);
                    return;
                }

                // Commit the characters we got to the buffer.
                buffer.commit(static_cast<std::size_t>(is.gcount()));
            }
            else
            {
                // Inform the parser that we've reached the end of the istream.
                p.put_eof(ec);
                if(ec)
                    return;
                break;
            }
        }

        // Write the data to the parser
        auto const bytes_used = p.put(buffer.data(), ec);

        // This error means that the parser needs additional octets.
        if(ec == error::need_more)
            ec = {};
        if(ec)
            return;

        // Consume the buffer octets that were actually parsed.
        buffer.consume(bytes_used);
    }
    while(! p.is_done());

    // Transfer ownership of the message container in the parser to the caller.
    msg = p.release();
}

//]

//------------------------------------------------------------------------------
//
// Example: Deferred Body Type
//
//------------------------------------------------------------------------------

//[example_http_defer_body

/** Handle a form POST request, choosing a body type depending on the Content-Type.

    This reads a request from the input stream. If the method is POST, and
    the Content-Type is "application/x-www-form-urlencoded  " or
    "multipart/form-data", a `string_body` is used to receive and store
    the message body. Otherwise, a `dynamic_body` is used to store the message
    body. After the request is received, the handler will be invoked with the
    request.

    @param stream The stream to read from.

    @param buffer The buffer to use for reading.

    @param handler The handler to invoke when the request is complete.
    The handler must be invokable with this signature:
    @code
    template<class Body>
    void handler(request<Body>&& req);
    @endcode

    @throws system_error Thrown on failure.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    class Handler>
void
do_form_request(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    Handler&& handler)
{
    // Start with an empty_body parser
    request_parser<empty_body> req0;

    // Read just the header. Otherwise, the empty_body
    // would generate an error if body octets were received.
    read_header(stream, buffer, req0);

    // Choose a body depending on the method verb
    switch(req0.get().method())
    {
    case verb::post:
    {
        // If this is not a form upload then use a string_body
        if( req0.get()[field::content_type] != "application/x-www-form-urlencoded" &&
            req0.get()[field::content_type] != "multipart/form-data")
            goto do_dynamic_body;

        // Commit to string_body as the body type.
        // As long as there are no body octets in the parser
        // we are constructing from, no exception is thrown.
        request_parser<string_body> req{std::move(req0)};

        // Finish reading the message
        read(stream, buffer, req);

        // Call the handler. It can take ownership
        // if desired, since we are calling release()
        handler(req.release());
        break;
    }

    do_dynamic_body:
    default:
    {
        // Commit to dynamic_body as the body type.
        // As long as there are no body octets in the parser
        // we are constructing from, no exception is thrown.
        request_parser<dynamic_body> req{std::move(req0)};

        // Finish reading the message
        read(stream, buffer, req);

        // Call the handler. It can take ownership
        // if desired, since we are calling release()
        handler(req.release());
        break;
    }
    }
}

//]



//------------------------------------------------------------------------------
//
// Example: Custom Parser
//
//------------------------------------------------------------------------------

//[example_http_custom_parser

template<bool isRequest>
class custom_parser
    : public basic_parser<isRequest, custom_parser<isRequest>>
{
private:
    // The friend declaration is needed,
    // otherwise the callbacks must be made public.
    friend class basic_parser<isRequest, custom_parser>;

    /// Called after receiving the request-line (isRequest == true).
    void
    on_request_impl(
        verb method,                // The method verb, verb::unknown if no match
        string_view method_str,     // The method as a string
        string_view target,         // The request-target
        int version,                // The HTTP-version
        error_code& ec);            // The error returned to the caller, if any

    /// Called after receiving the start-line (isRequest == false).
    void
    on_response_impl(
        int code,                   // The status-code
        string_view reason,         // The obsolete reason-phrase
        int version,                // The HTTP-version
        error_code& ec);            // The error returned to the caller, if any

    /// Called after receiving a header field.
    void
    on_field_impl(
        field f,                    // The known-field enumeration constant
        string_view name,           // The field name string.
        string_view value,          // The field value
        error_code& ec);            // The error returned to the caller, if any

    /// Called after the complete header is received.
    void
    on_header_impl(
        error_code& ec);            // The error returned to the caller, if any

    /// Called just before processing the body, if a body exists.
    void
    on_body_init_impl(
        boost::optional<
            std::uint64_t> const&
                content_length,     // Content length if known, else `boost::none`
        error_code& ec);            // The error returned to the caller, if any

    /// Called for each piece of the body, if a body exists.
    //!
    //! This is used when there is no chunked transfer coding.
    //!
    //! The function returns the number of bytes consumed from the
    //! input buffer. Any input octets not consumed will be will be
    //! presented on subsequent calls.
    //!
    std::size_t
    on_body_impl(
        string_view s,              // A portion of the body
        error_code& ec);            // The error returned to the caller, if any

    /// Called for each chunk header.
    void
    on_chunk_header_impl(
        std::uint64_t size,         // The size of the upcoming chunk,
                                    // or zero for the last chunk
        string_view extension,      // The chunk extensions (may be empty)
        error_code& ec);            // The error returned to the caller, if any

    /// Called to deliver the chunk body.
    //!
    //! This is used when there is a chunked transfer coding. The
    //! implementation will automatically remove the encoding before
    //! calling this function.
    //!
    //! The function returns the number of bytes consumed from the
    //! input buffer. Any input octets not consumed will be will be
    //! presented on subsequent calls.
    //!
    std::size_t
    on_chunk_body_impl(
        std::uint64_t remain,       // The number of bytes remaining in the chunk,
                                    // including what is being passed here.
                                    // or zero for the last chunk
        string_view body,           // The next piece of the chunk body
        error_code& ec);            // The error returned to the caller, if any

    /// Called when the complete message is parsed.
    void
    on_finish_impl(error_code& ec);

public:
    custom_parser() = default;
};

//]

// Definitions are not part of the docs but necessary to link

template<bool isRequest>
void custom_parser<isRequest>::
on_request_impl(verb method, string_view method_str,
    string_view path, int version, error_code& ec)
{
    boost::ignore_unused(method, method_str, path, version);
    ec = {};
}

template<bool isRequest>
void custom_parser<isRequest>::
on_response_impl(
    int status,
    string_view reason,
    int version,
    error_code& ec)
{
    boost::ignore_unused(status, reason, version);
    ec = {};
}

template<bool isRequest>
void custom_parser<isRequest>::
on_field_impl(
    field f,
    string_view name,
    string_view value,
    error_code& ec)
{
    boost::ignore_unused(f, name, value);
    ec = {};
}

template<bool isRequest>
void custom_parser<isRequest>::
on_header_impl(error_code& ec)
{
    ec = {};
}

template<bool isRequest>
void custom_parser<isRequest>::
on_body_init_impl(
    boost::optional<std::uint64_t> const& content_length,
    error_code& ec)
{
    boost::ignore_unused(content_length);
    ec = {};
}

template<bool isRequest>
std::size_t custom_parser<isRequest>::
on_body_impl(string_view body, error_code& ec)
{
    boost::ignore_unused(body);
    ec = {};
    return body.size();
}

template<bool isRequest>
void custom_parser<isRequest>::
on_chunk_header_impl(
    std::uint64_t size,
    string_view extension,
    error_code& ec)
{
    boost::ignore_unused(size, extension);
    ec = {};
}

template<bool isRequest>
std::size_t custom_parser<isRequest>::
on_chunk_body_impl(
    std::uint64_t remain,
    string_view body,
    error_code& ec)
{
    boost::ignore_unused(remain);
    ec = {};
    return body.size();
}

template<bool isRequest>
void custom_parser<isRequest>::
on_finish_impl(error_code& ec)
{
    ec = {};
}

//------------------------------------------------------------------------------
//
// Example: Incremental Read
//
//------------------------------------------------------------------------------

//[example_incremental_read

/*  This function reads a message using a fixed size buffer to hold
    portions of the body, and prints the body contents to a `std::ostream`.
*/
template<
    bool isRequest,
    class SyncReadStream,
    class DynamicBuffer>
void
read_and_print_body(
    std::ostream& os,
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    error_code& ec)
{
    parser<isRequest, buffer_body> p;
    read_header(stream, buffer, p, ec);
    if(ec)
        return;
    while(! p.is_done())
    {
        char buf[512];
        p.get().body().data = buf;
        p.get().body().size = sizeof(buf);
        read(stream, buffer, p, ec);
        if(ec == error::need_buffer)
            ec.assign(0, ec.category());
        if(ec)
            return;
        os.write(buf, sizeof(buf) - p.get().body().size);
    }
}

//]


//------------------------------------------------------------------------------
//
// Example: Expect 100-continue
//
//------------------------------------------------------------------------------

//[example_chunk_parsing

/** Read a message with a chunked body and print the chunks and extensions
*/
template<
    bool isRequest,
    class SyncReadStream,
    class DynamicBuffer>
void
print_chunked_body(
    std::ostream& os,
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    error_code& ec)
{
    // Declare the parser with an empty body since
    // we plan on capturing the chunks ourselves.
    parser<isRequest, empty_body> p;

    // First read the complete header
    read_header(stream, buffer, p, ec);
    if(ec)
        return;

    // This container will hold the extensions for each chunk
    chunk_extensions ce;

    // This string will hold the body of each chunk
    std::string chunk;

    // Declare our chunk header callback  This is invoked
    // after each chunk header and also after the last chunk.
    auto header_cb =
    [&](std::uint64_t size,         // Size of the chunk, or zero for the last chunk
        string_view extensions,     // The raw chunk-extensions string. Already validated.
        error_code& ev)             // We can set this to indicate an error
    {
        // Parse the chunk extensions so we can access them easily
        ce.parse(extensions, ev);
        if(ev)
            return;

        // See if the chunk is too big
        if(size > (std::numeric_limits<std::size_t>::max)())
        {
            ev = error::body_limit;
            return;
        }

        // Make sure we have enough storage, and
        // reset the container for the upcoming chunk
        chunk.reserve(static_cast<std::size_t>(size));
        chunk.clear();
    };

    // Set the callback. The function requires a non-const reference so we
    // use a local variable, since temporaries can only bind to const refs.
    p.on_chunk_header(header_cb);

    // Declare the chunk body callback. This is called one or
    // more times for each piece of a chunk body.
    auto body_cb =            
    [&](std::uint64_t remain,   // The number of bytes left in this chunk
        string_view body,       // A buffer holding chunk body data
        error_code& ec)         // We can set this to indicate an error
    {
        // If this is the last piece of the chunk body,
        // set the error so that the call to `read` returns
        // and we can process the chunk.
        if(remain == body.size())
            ec = error::end_of_chunk;

        // Append this piece to our container
        chunk.append(body.data(), body.size());

        // The return value informs the parser of how much of the body we
        // consumed. We will indicate that we consumed everything passed in.
        return body.size();
    };
    p.on_chunk_body(body_cb);

    while(! p.is_done())
    {   
        // Read as much as we can. When we reach the end of the chunk, the chunk
        // body callback will make the read return with the end_of_chunk error.
        read(stream, buffer, p, ec);
        if(! ec)
            continue;
        else if(ec != error::end_of_chunk)
            return;
        else
            ec.assign(0, ec.category());

        // We got a whole chunk, print the extensions:
        for(auto const& extension : ce)
        {
            os << "Extension: " << extension.first;
            if(! extension.second.empty())
                os << " = " << extension.second << std::endl;
            else
                os << std::endl;
        }

        // Now print the chunk body
        os << "Chunk Body: " << chunk << std::endl;
    }

    // Get a reference to the parsed message, this is for convenience
    auto const& msg = p.get();

    // Check each field promised in the "Trailer" header and output it
    for(auto const& name : token_list{msg[field::trailer]})
    {
        // Find the trailer field
        auto it = msg.find(name);
        if(it == msg.end())
        {
            // Oops! They promised the field but failed to deliver it
            os << "Missing Trailer: " << name << std::endl;
            continue;
        }
        os << it->name() << ": " << it->value() << std::endl;
    }
}

//]

} // http
} // beast
} // boost
