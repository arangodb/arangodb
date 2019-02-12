//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_READ_HPP
#define BOOST_BEAST_HTTP_READ_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/basic_parser.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/asio/async_result.hpp>

namespace boost {
namespace beast {
namespace http {

/** Read part of a message from a stream using a parser.

    This function is used to read part of a message from a stream into a
    subclass of @ref basic_parser.
    The call will block until one of the following conditions is true:

    @li A call to @ref basic_parser::put with a non-empty buffer sequence
    is successful.

    @li An error occurs.

    This operation is implemented in terms of one or
    more calls to the stream's `read_some` function.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @return The number of bytes transferred to the parser.

    @throws system_error Thrown on failure.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser);

/** Read part of a message from a stream using a parser.

    This function is used to read part of a message from a stream into a
    subclass of @ref basic_parser.
    The call will block until one of the following conditions is true:

    @li A call to @ref basic_parser::put with a non-empty buffer sequence
    is successful.

    @li An error occurs.

    This operation is implemented in terms of one or
    more calls to the stream's `read_some` function.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    The function returns the number of bytes processed from the dynamic
    buffer. The caller should remove these bytes by calling `consume` on
    the dynamic buffer, regardless of any error.

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @param ec Set to the error, if any occurred.

    @return The number of bytes transferred to the parser.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec);

/** Read part of a message asynchronously from a stream using a parser.

    This function is used to asynchronously read part of a message from
    a stream into a subclass of @ref basic_parser.
    The function call always returns immediately. The asynchronous operation
    will continue until one of the following conditions is true:

    @li A call to @ref basic_parser::put with a non-empty buffer sequence
    is successful.

    @li An error occurs.

    This operation is implemented in terms of zero or more calls to
    the next layer's `async_read_some` function, and is known as a
    <em>composed operation</em>. The program must ensure that the
    stream performs no other reads until this operation completes.
    The implementation may read additional octets that lie past the
    end of the object being parsed. This additional data is stored
    in the stream buffer, which may be used in subsequent calls.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b AsyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.
    The object must remain valid at least until the
    handler is called; ownership is not transferred.

    @param handler Invoked when the operation completes.
    The handler may be moved or copied as needed.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error,        // result of operation
        std::size_t bytes_transferred   // the number of bytes transferred to the parser
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_context::post`.

    The completion handler will receive as a parameter the number
    of octets processed from the dynamic buffer. The octets should
    be removed by calling `consume` on the dynamic buffer after
    the read completes, regardless of any error.
*/
template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_some(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler);

//------------------------------------------------------------------------------

/** Read a header from a stream using a parser.

    This function is used to read a header from a stream into a subclass
    of @ref basic_parser.
    The call will block until one of the following conditions is true:

    @li @ref basic_parser::is_header_done returns `true`

    @li An error occurs.

    This operation is implemented in terms of one or
    more calls to the stream's `read_some` function.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @return The number of bytes transferred to the parser.

    @throws system_error Thrown on failure.

    @note The implementation will call @ref basic_parser::eager
    with the value `false` on the parser passed in.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_header(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser);

/** Read a header from a stream using a parser.

    This function is used to read a header from a stream into a subclass
    of @ref basic_parser.
    The call will block until one of the following conditions is true:

    @li @ref basic_parser::is_header_done returns `true`

    @li An error occurs.

    This operation is implemented in terms of one or
    more calls to the stream's `read_some` function.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @param ec Set to the error, if any occurred.

    @return The number of bytes transferred to the parser.

    @note The implementation will call @ref basic_parser::eager
    with the value `false` on the parser passed in.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_header(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec);

/** Read a header from a stream asynchronously using a parser.

    This function is used to asynchronously read a header from a stream
    into a subclass of @ref basic_parser.
    The function call always returns immediately. The asynchronous operation
    will continue until one of the following conditions is true:

    @li @ref basic_parser::is_header_done returns `true`

    @li An error occurs.

    This operation is implemented in terms of one or more calls to
    the stream's `async_read_some` function, and is known as a
    <em>composed operation</em>. The program must ensure that the
    stream performs no other reads until this operation completes.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b AsyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.
    The object must remain valid at least until the
    handler is called; ownership is not transferred.

    @param handler Invoked when the operation completes.
    The handler may be moved or copied as needed.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error,        // result of operation,
        std::size_t bytes_transferred   // the number of bytes transferred to the parser
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_context::post`.

    @note The implementation will call @ref basic_parser::eager
    with the value `false` on the parser passed in.
*/
template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_header(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler);

//------------------------------------------------------------------------------

/** Read a complete message from a stream using a parser.

    This function is used to read a complete message from a stream into a
    subclass of @ref basic_parser.
    The call will block until one of the following conditions is true:

    @li @ref basic_parser::is_done returns `true`

    @li An error occurs.

    This operation is implemented in terms of one or
    more calls to the stream's `read_some` function.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @return The number of bytes transferred to the parser.

    @throws system_error Thrown on failure.

    @note The implementation will call @ref basic_parser::eager
    with the value `true` on the parser passed in.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser);

/** Read a complete message from a stream using a parser.

    This function is used to read a complete message from a stream into a
    subclass of @ref basic_parser.
    The call will block until one of the following conditions is true:

    @li @ref basic_parser::is_done returns `true`

    @li An error occurs.

    This operation is implemented in terms of one or
    more calls to the stream's `read_some` function.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @param ec Set to the error, if any occurred.

    @return The number of bytes transferred to the parser.

    @note The implementation will call @ref basic_parser::eager
    with the value `true` on the parser passed in.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec);

/** Read a complete message from a stream asynchronously using a parser.

    This function is used to asynchronously read a complete message from a
    stream into a subclass of @ref basic_parser.
    The function call always returns immediately. The asynchronous operation
    will continue until one of the following conditions is true:

    @li @ref basic_parser::is_done returns `true`

    @li An error occurs.

    This operation is implemented in terms of one or more calls to
    the stream's `async_read_some` function, and is known as a
    <em>composed operation</em>. The program must ensure that the
    stream performs no other reads until this operation completes.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b AsyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.
    The object must remain valid at least until the
    handler is called; ownership is not transferred.

    @param handler Invoked when the operation completes.
    The handler may be moved or copied as needed.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error,        // result of operation,
        std::size_t bytes_transferred   // the number of bytes transferred to the parser
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_context::post`.

    @note The implementation will call @ref basic_parser::eager
    with the value `true` on the parser passed in.
*/
template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler);

//------------------------------------------------------------------------------

/** Read a complete message from a stream.

    This function is used to read a complete message from a stream using HTTP/1.
    The call will block until one of the following conditions is true:

    @li The entire message is read.

    @li An error occurs.

    This operation is implemented in terms of one or
    more calls to the stream's `read_some` function.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param msg An object in which to store the message contents.
    This object should not have previous contents, otherwise
    the behavior is undefined.
    The type must be @b MoveAssignable and @b MoveConstructible.

    @return The number of bytes transferred to the parser.

    @throws system_error Thrown on failure.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg);

/** Read a complete message from a stream.

    This function is used to read a complete message from a stream using HTTP/1.
    The call will block until one of the following conditions is true:

    @li The entire message is read.

    @li An error occurs.

    This operation is implemented in terms of one or
    more calls to the stream's `read_some` function.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param msg An object in which to store the message contents.
    This object should not have previous contents, otherwise
    the behavior is undefined.
    The type must be @b MoveAssignable and @b MoveConstructible.

    @param ec Set to the error, if any occurred.

    @return The number of bytes transferred to the parser.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg,
    error_code& ec);

/** Read a complete message from a stream asynchronously.

    This function is used to asynchronously read a complete message from a
    stream using HTTP/1.
    The function call always returns immediately. The asynchronous operation
    will continue until one of the following conditions is true:

    @li The entire message is read.

    @li An error occurs.

    This operation is implemented in terms of one or more calls to
    the stream's `async_read_some` function, and is known as a
    <em>composed operation</em>. The program must ensure that the
    stream performs no other reads until this operation completes.
    The implementation may read additional octets that lie past the
    end of the message being read. This additional data is stored
    in the dynamic buffer, which must be retained for subsequent reads.

    If the stream returns the error `boost::asio::error::eof` indicating the
    end of file during a read, the error returned from this function will be:

    @li @ref error::end_of_stream if no octets were parsed, or

    @li @ref error::partial_message if any octets were parsed but the
    message was incomplete, otherwise:

    @li A successful result. A subsequent attempt to read will
    return @ref error::end_of_stream

    @param stream The stream from which the data is to be read.
    The type must support the @b AsyncReadStream concept.

    @param buffer A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param msg An object in which to store the message contents.
    This object should not have previous contents, otherwise
    the behavior is undefined.
    The type must be @b MoveAssignable and @b MoveConstructible.

    The object must remain valid at least until the
    handler is called; ownership is not transferred.

    @param handler Invoked when the operation completes.
    The handler may be moved or copied as needed.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error,        // result of operation,
        std::size_t bytes_transferred   // the number of bytes transferred to the parser
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_context::post`.
*/
template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg,
    ReadHandler&& handler);

} // http
} // beast
} // boost

#include <boost/beast/http/impl/read.ipp>

#endif
