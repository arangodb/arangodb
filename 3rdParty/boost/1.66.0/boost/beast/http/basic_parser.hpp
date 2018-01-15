//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_BASIC_PARSER_HPP
#define BOOST_BEAST_HTTP_BASIC_PARSER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/detail/basic_parser.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <boost/assert.hpp>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace http {

/** A parser for decoding HTTP/1 wire format messages.

    This parser is designed to efficiently parse messages in the
    HTTP/1 wire format. It allocates no memory when input is
    presented as a single contiguous buffer, and uses minimal
    state. It will handle chunked encoding and it understands
    the semantics of the Connection, Content-Length, and Upgrade
    fields.
    The parser is optimized for the case where the input buffer
    sequence consists of a single contiguous buffer. The
    @ref flat_buffer class is provided, which guarantees
    that the input sequence of the stream buffer will be represented
    by exactly one contiguous buffer. To ensure the optimum performance
    of the parser, use @ref flat_buffer with HTTP algorithms
    such as @ref read, @ref read_some, @ref async_read, and @ref async_read_some.
    Alternatively, the caller may use custom techniques to ensure that
    the structured portion of the HTTP message (header or chunk header)
    is contained in a linear buffer.

    The interface uses CRTP (Curiously Recurring Template Pattern).
    To use this class directly, derive from @ref basic_parser. When
    bytes are presented, the implementation will make a series of zero
    or more calls to derived class members functions (termed "callbacks"
    in this context) matching a specific signature.

    Every callback must be provided by the derived class, or else
    a compilation error will be generated. This exemplar shows
    the signature and description of the callbacks required in
    the derived class.
    For each callback, the function will ensure that `!ec` is `true`
    if there was no error or set to the appropriate error code if
    there was one.  If an error is set, the value is propagated to
    the caller of the parser.

    @par Derived Class Requirements
    @code
        template<bool isRequest>
        class derived
            : public basic_parser<isRequest, derived<isRequest>>
        {
        private:
            // The friend declaration is needed,
            // otherwise the callbacks must be made public.
            friend class basic_parser<isRequest, derived>;

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
            derived() = default;
        };
    @endcode

    @tparam isRequest A `bool` indicating whether the parser will be
    presented with request or response message.

    @tparam Derived The derived class type. This is part of the
    Curiously Recurring Template Pattern interface.

    @note If the parser encounters a field value with obs-fold
    longer than 4 kilobytes in length, an error is generated.
*/
template<bool isRequest, class Derived>
class basic_parser
    : private detail::basic_parser_base
{
    template<bool OtherIsRequest, class OtherDerived>
    friend class basic_parser;

    // limit on the size of the stack flat buffer
    static std::size_t constexpr max_stack_buffer = 8192;

    // Message will be complete after reading header
    static unsigned constexpr flagSkipBody              = 1<<  0;

    // Consume input buffers across semantic boundaries
    static unsigned constexpr flagEager                 = 1<<  1;

    // The parser has read at least one byte
    static unsigned constexpr flagGotSome               = 1<<  2;

    // Message semantics indicate a body is expected.
    // cleared if flagSkipBody set
    //
    static unsigned constexpr flagHasBody               = 1<<  3;

    static unsigned constexpr flagHTTP11                = 1<<  4;
    static unsigned constexpr flagNeedEOF               = 1<<  5;
    static unsigned constexpr flagExpectCRLF            = 1<<  6;
    static unsigned constexpr flagConnectionClose       = 1<<  7;
    static unsigned constexpr flagConnectionUpgrade     = 1<<  8;
    static unsigned constexpr flagConnectionKeepAlive   = 1<<  9;
    static unsigned constexpr flagContentLength         = 1<< 10;
    static unsigned constexpr flagChunked               = 1<< 11;
    static unsigned constexpr flagUpgrade               = 1<< 12;
    static unsigned constexpr flagFinalChunk            = 1<< 13;

    static constexpr
    std::uint64_t
    default_body_limit(std::true_type)
    {
        // limit for requests
        return 1 * 1024 * 1024; // 1MB
    }

    static constexpr
    std::uint64_t
    default_body_limit(std::false_type)
    {
        // limit for responses
        return 8 * 1024 * 1024; // 8MB
    }

    std::uint64_t body_limit_ =
        default_body_limit(is_request{});   // max payload body
    std::uint64_t len_ = 0;                 // size of chunk or body
    std::unique_ptr<char[]> buf_;           // temp storage
    std::size_t buf_len_ = 0;               // size of buf_
    std::size_t skip_ = 0;                  // resume search here
    std::uint32_t header_limit_ = 8192;     // max header size
    unsigned short status_ = 0;             // response status
    state state_ = state::nothing_yet;      // initial state
    unsigned f_ = 0;                        // flags

protected:
    /// Default constructor
    basic_parser() = default;

    /// Move constructor
    basic_parser(basic_parser &&) = default;

    /// Move assignment
    basic_parser& operator=(basic_parser &&) = default;

    /** Move constructor

        @note

        After the move, the only valid operation on the
        moved-from object is destruction.
    */
    template<class OtherDerived>
    basic_parser(basic_parser<isRequest, OtherDerived>&&);

public:
    /// `true` if this parser parses requests, `false` for responses.
    using is_request =
        std::integral_constant<bool, isRequest>;

    /// Destructor
    ~basic_parser() = default;

    /// Copy constructor
    basic_parser(basic_parser const&) = delete;

    /// Copy assignment
    basic_parser& operator=(basic_parser const&) = delete;

    /** Returns a reference to this object as a @ref basic_parser.

        This is used to pass a derived class where a base class is
        expected, to choose a correct function overload when the
        resolution would be ambiguous.
    */
    basic_parser&
    base()
    {
        return *this;
    }

    /** Returns a constant reference to this object as a @ref basic_parser.

        This is used to pass a derived class where a base class is
        expected, to choose a correct function overload when the
        resolution would be ambiguous.
    */
    basic_parser const&
    base() const
    {
        return *this;
    }

    /// Returns `true` if the parser has received at least one byte of input.
    bool
    got_some() const
    {
        return state_ != state::nothing_yet;
    }

    /** Returns `true` if the message is complete.

        The message is complete after the full header is prduced
        and one of the following is true:

        @li The skip body option was set.

        @li The semantics of the message indicate there is no body.

        @li The semantics of the message indicate a body is expected,
        and the entire body was parsed.
    */
    bool
    is_done() const
    {
        return state_ == state::complete;
    }

    /** Returns `true` if a the parser has produced the full header.
    */
    bool
    is_header_done() const
    {
        return state_ > state::fields;
    }

    /** Returns `true` if the message is an upgrade message.

        @note The return value is undefined unless
        @ref is_header_done would return `true`.
    */
    bool
    upgrade() const
    {
        return (f_ & flagConnectionUpgrade) != 0;
    }

    /** Returns `true` if the last value for Transfer-Encoding is "chunked".

        @note The return value is undefined unless
        @ref is_header_done would return `true`.
    */
    bool
    chunked() const
    {
        return (f_ & flagChunked) != 0;
    }

    /** Returns `true` if the message has keep-alive connection semantics.

        This function always returns `false` if @ref need_eof would return
        `false`.

        @note The return value is undefined unless
        @ref is_header_done would return `true`.
    */
    bool
    keep_alive() const;

    /** Returns the optional value of Content-Length if known.

        @note The return value is undefined unless
        @ref is_header_done would return `true`.
    */
    boost::optional<std::uint64_t>
    content_length() const;

    /** Returns `true` if the message semantics require an end of file.

        Depending on the contents of the header, the parser may
        require and end of file notification to know where the end
        of the body lies. If this function returns `true` it will be
        necessary to call @ref put_eof when there will never be additional
        data from the input.
    */
    bool
    need_eof() const
    {
        return (f_ & flagNeedEOF) != 0;
    }

    /** Set the limit on the payload body.

        This function sets the maximum allowed size of the payload body,
        before any encodings except chunked have been removed. Depending
        on the message semantics, one of these cases will apply:

        @li The Content-Length is specified and exceeds the limit. In
        this case the result @ref error::body_limit is returned
        immediately after the header is parsed.

        @li The Content-Length is unspecified and the chunked encoding
        is not specified as the last encoding. In this case the end of
        message is determined by the end of file indicator on the
        associated stream or input source. If a sufficient number of
        body payload octets are presented to the parser to exceed the
        configured limit, the parse fails with the result
        @ref error::body_limit

        @li The Transfer-Encoding specifies the chunked encoding as the
        last encoding. In this case, when the number of payload body
        octets produced by removing the chunked encoding  exceeds
        the configured limit, the parse fails with the result
        @ref error::body_limit.
        
        Setting the limit after any body octets have been parsed
        results in undefined behavior.

        The default limit is 1MB for requests and 8MB for responses.

        @param v The payload body limit to set
    */
    void
    body_limit(std::uint64_t v)
    {
        body_limit_ = v;
    }

    /** Set a limit on the total size of the header.

        This function sets the maximum allowed size of the header
        including all field name, value, and delimiter characters
        and also including the CRLF sequences in the serialized
        input. If the end of the header is not found within the
        limit of the header size, the error @ref error::header_limit
        is returned by @ref put.

        Setting the limit after any header octets have been parsed
        results in undefined behavior.
    */
    void
    header_limit(std::uint32_t v)
    {
        header_limit_ = v;
    }

    /// Returns `true` if the eager parse option is set.
    bool
    eager() const
    {
        return (f_ & flagEager) != 0;
    }

    /** Set the eager parse option.

        Normally the parser returns after successfully parsing a structured
        element (header, chunk header, or chunk body) even if there are octets
        remaining in the input. This is necessary when attempting to parse the
        header first, or when the caller wants to inspect information which may
        be invalidated by subsequent parsing, such as a chunk extension. The
        `eager` option controls whether the parser keeps going after parsing
        structured element if there are octets remaining in the buffer and no
        error occurs. This option is automatically set or cleared during certain
        stream operations to improve performance with no change in functionality.

        The default setting is `false`.

        @param v `true` to set the eager parse option or `false` to disable it.
    */
    void
    eager(bool v)
    {
        if(v)
            f_ |= flagEager;
        else
            f_ &= ~flagEager;
    }

    /// Returns `true` if the skip parse option is set.
    bool
    skip() const
    {
        return (f_ & flagSkipBody) != 0;
    }

    /** Set the skip parse option.

        This option controls whether or not the parser expects to see an HTTP
        body, regardless of the presence or absence of certain fields such as
        Content-Length or a chunked Transfer-Encoding. Depending on the request,
        some responses do not carry a body. For example, a 200 response to a
        CONNECT request from a tunneling proxy, or a response to a HEAD request.
        In these cases, callers may use this function inform the parser that
        no body is expected. The parser will consider the message complete
        after the header has been received.

        @param v `true` to set the skip body option or `false` to disable it.

        @note This function must called before any bytes are processed.
    */
    void
    skip(bool v);

    /** Write a buffer sequence to the parser.

        This function attempts to incrementally parse the HTTP
        message data stored in the caller provided buffers. Upon
        success, a positive return value indicates that the parser
        made forward progress, consuming that number of
        bytes.

        In some cases there may be an insufficient number of octets
        in the input buffer in order to make forward progress. This
        is indicated by the code @ref error::need_more. When
        this happens, the caller should place additional bytes into
        the buffer sequence and call @ref put again.

        The error code @ref error::need_more is special. When this
        error is returned, a subsequent call to @ref put may succeed
        if the buffers have been updated. Otherwise, upon error
        the parser may not be restarted.

        @param buffers An object meeting the requirements of
        @b ConstBufferSequence that represents the next chunk of
        message data. If the length of this buffer sequence is
        one, the implementation will not allocate additional memory.
        The class @ref beast::flat_buffer is provided as one way to
        meet this requirement

        @param ec Set to the error, if any occurred.

        @return The number of octets consumed in the buffer
        sequence. The caller should remove these octets even if the
        error is set.
    */
    template<class ConstBufferSequence>
    std::size_t
    put(ConstBufferSequence const& buffers, error_code& ec);

#if ! BOOST_BEAST_DOXYGEN
    std::size_t
    put(boost::asio::const_buffer const& buffer,
        error_code& ec);
#endif

    /** Inform the parser that the end of stream was reached.

        In certain cases, HTTP needs to know where the end of
        the stream is. For example, sometimes servers send
        responses without Content-Length and expect the client
        to consume input (for the body) until EOF. Callbacks
        and errors will still be processed as usual.

        This is typically called when a read from the
        underlying stream object sets the error code to
        `boost::asio::error::eof`.

        @note Only valid after parsing a complete header.

        @param ec Set to the error, if any occurred. 
    */
    void
    put_eof(error_code& ec);

private:
    inline
    Derived&
    impl()
    {
        return *static_cast<Derived*>(this);
    }

    template<class ConstBufferSequence>
    std::size_t
    put_from_stack(std::size_t size,
        ConstBufferSequence const& buffers,
            error_code& ec);

    void
    maybe_need_more(
        char const* p, std::size_t n,
            error_code& ec);

    void
    parse_start_line(
        char const*& p, char const* last,
            error_code& ec, std::true_type);

    void
    parse_start_line(
        char const*& p, char const* last,
            error_code& ec, std::false_type);

    void
    parse_fields(
        char const*& p, char const* last,
            error_code& ec);

    void
    finish_header(
        error_code& ec, std::true_type);

    void
    finish_header(
        error_code& ec, std::false_type);

    void
    parse_body(char const*& p,
        std::size_t n, error_code& ec);

    void
    parse_body_to_eof(char const*& p,
        std::size_t n, error_code& ec);

    void
    parse_chunk_header(char const*& p,
        std::size_t n, error_code& ec);

    void
    parse_chunk_body(char const*& p,
        std::size_t n, error_code& ec);

    void
    do_field(field f,
        string_view value, error_code& ec);
};

} // http
} // beast
} // boost

#include <boost/beast/http/impl/basic_parser.ipp>

#endif
