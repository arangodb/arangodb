//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_PARSER_HPP
#define BOOST_BEAST_HTTP_PARSER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/http/basic_parser.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/type_traits.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace http {

/** An HTTP/1 parser for producing a message.

    This class uses the basic HTTP/1 wire format parser to convert
    a series of octets into a @ref message using the @ref basic_fields
    container to represent the fields.

    @tparam isRequest Indicates whether a request or response
    will be parsed.

    @tparam Body The type used to represent the body. This must
    meet the requirements of @b Body.

    @tparam Allocator The type of allocator used with the
    @ref basic_fields container.

    @note A new instance of the parser is required for each message.
*/
template<
    bool isRequest,
    class Body,
    class Allocator = std::allocator<char>>
class parser
    : public basic_parser<isRequest,
        parser<isRequest, Body, Allocator>>
{
    static_assert(is_body<Body>::value,
        "Body requirements not met");

    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");

    template<bool, class, class>
    friend class parser;

    using base_type = basic_parser<isRequest,
        parser<isRequest, Body, Allocator>>;

    message<isRequest, Body, basic_fields<Allocator>> m_;
    typename Body::reader rd_;
    bool rd_inited_ = false;

    std::function<void(
        std::uint64_t,
        string_view,
        error_code&)> cb_h_;

    std::function<std::size_t(
        std::uint64_t,
        string_view,
        error_code&)> cb_b_;

public:
    /// The type of message returned by the parser
    using value_type =
        message<isRequest, Body, basic_fields<Allocator>>;

    /// Destructor
    ~parser() = default;

    /// Constructor (disallowed)
    parser(parser const&) = delete;

    /// Assignment (disallowed)
    parser& operator=(parser const&) = delete;

    /// Constructor (disallowed)
    parser(parser&& other) = delete;

    /// Constructor
    parser();

    /** Constructor

        @param args Optional arguments forwarded to the 
        @ref http::message constructor.

        @note This function participates in overload
        resolution only if the first argument is not a
        @ref parser.
    */
#if BOOST_BEAST_DOXYGEN
    template<class... Args>
    explicit
    parser(Args&&... args);
#else
    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            ! detail::is_parser<typename
                std::decay<Arg1>::type>::value>::type>
    explicit
    parser(Arg1&& arg1, ArgN&&... argn);
#endif

    /** Construct a parser from another parser, changing the Body type.

        This constructs a new parser by move constructing the
        header from another parser with a different body type. The
        constructed-from parser must not have any parsed body octets or
        initialized @b BodyReader, otherwise an exception is generated.

        @par Example
        @code
        // Deferred body type commitment
        request_parser<empty_body> req0;
        ...
        request_parser<string_body> req{std::move(req0)};
        @endcode

        If an exception is thrown, the state of the constructed-from
        parser is undefined.

        @param parser The other parser to construct from. After
        this call returns, the constructed-from parser may only
        be destroyed.

        @param args Optional arguments forwarded to the message
        constructor.

        @throws std::invalid_argument Thrown when the constructed-from
        parser has already initialized a body reader.

        @note This function participates in overload resolution only
        if the other parser uses a different body type.
    */
#if BOOST_BEAST_DOXYGEN
    template<class OtherBody, class... Args>
#else
    template<class OtherBody, class... Args,
        class = typename std::enable_if<
            ! std::is_same<Body, OtherBody>::value>::type>
#endif
    explicit
    parser(parser<isRequest, OtherBody,
        Allocator>&& parser, Args&&... args);

    /** Returns the parsed message.

        Depending on the parser's progress,
        parts of this object may be incomplete.
    */
    value_type const&
    get() const
    {
        return m_;
    }

    /** Returns the parsed message.

        Depending on the parser's progress,
        parts of this object may be incomplete.
    */
    value_type&
    get()
    {
        return m_;
    }

    /** Returns ownership of the parsed message.

        Ownership is transferred to the caller.
        Depending on the parser's progress,
        parts of this object may be incomplete.

        @par Requires

        @ref value_type is @b MoveConstructible
    */
    value_type
    release()
    {
        static_assert(std::is_move_constructible<decltype(m_)>::value,
            "MoveConstructible requirements not met");
        return std::move(m_);
    }

    /** Set a callback to be invoked on each chunk header.

        The callback will be invoked once for every chunk in the message
        payload, as well as once for the last chunk. The invocation 
        happens after the chunk header is available but before any body
        octets have been parsed.

        The extensions are provided in raw, validated form, use
        @ref chunk_extensions::parse to parse the extensions into a
        structured container for easier access.
        The implementation type-erases the callback without requiring
        a dynamic allocation. For this reason, the callback object is
        passed by a non-constant reference.

        @par Example
        @code
        auto callback =
            [](std::uint64_t size, string_view extensions, error_code& ec)
            {
                //...
            };
        parser.on_chunk_header(callback);
        @endcode

        @param cb The function to set, which must be invocable with
        this equivalent signature:
        @code
        void
        on_chunk_header(
            std::uint64_t size,         // Size of the chunk, zero for the last chunk
            string_view extensions,     // The chunk-extensions in raw form
            error_code& ec);            // May be set by the callback to indicate an error
        @endcode
    */
    template<class Callback>
    void
    on_chunk_header(Callback& cb)
    {
        // Callback may not be constant, caller is responsible for
        // managing the lifetime of the callback. Copies are not made.
        BOOST_STATIC_ASSERT(! std::is_const<Callback>::value);

        // Can't set the callback after receiving any chunk data!
        BOOST_ASSERT(! rd_inited_);

        cb_h_ = std::ref(cb);
    }

    /** Set a callback to be invoked on chunk body data

        The provided function object will be invoked one or more times
        to provide buffers corresponding to the chunk body for the current
        chunk. The callback receives the number of octets remaining in this
        chunk body including the octets in the buffer provided.

        The callback must return the number of octets actually consumed.
        Any octets not consumed will be presented again in a subsequent
        invocation of the callback.
        The implementation type-erases the callback without requiring
        a dynamic allocation. For this reason, the callback object is
        passed by a non-constant reference.

        @par Example
        @code
        auto callback =
            [](std::uint64_t remain, string_view body, error_code& ec)
            {
                //...
            };
        parser.on_chunk_body(callback);
        @endcode

        @param cb The function to set, which must be invocable with
        this equivalent signature:
        @code
        std::size_t
        on_chunk_header(
            std::uint64_t remain,       // Octets remaining in this chunk, includes `body`
            string_view body,           // A buffer holding some or all of the remainder of the chunk body
            error_code& ec);            // May be set by the callback to indicate an error
        @endcode
        */
    template<class Callback>
    void
    on_chunk_body(Callback& cb)
    {
        // Callback may not be constant, caller is responsible for
        // managing the lifetime of the callback. Copies are not made.
        BOOST_STATIC_ASSERT(! std::is_const<Callback>::value);

        // Can't set the callback after receiving any chunk data!
        BOOST_ASSERT(! rd_inited_);

        cb_b_ = std::ref(cb);
    }

private:
    friend class basic_parser<isRequest, parser>;

    parser(std::true_type);
    parser(std::false_type);

    template<class OtherBody, class... Args,
        class = typename std::enable_if<
            ! std::is_same<Body, OtherBody>::value>::type>
    parser(
        std::true_type,
        parser<isRequest, OtherBody, Allocator>&& parser,
        Args&&... args);

    template<class OtherBody, class... Args,
        class = typename std::enable_if<
            ! std::is_same<Body, OtherBody>::value>::type>
    parser(
        std::false_type,
        parser<isRequest, OtherBody, Allocator>&& parser,
        Args&&... args);

    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            ! detail::is_parser<typename
                std::decay<Arg1>::type>::value>::type>
    explicit
    parser(Arg1&& arg1, std::true_type, ArgN&&... argn);

    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            ! detail::is_parser<typename
                std::decay<Arg1>::type>::value>::type>
    explicit
    parser(Arg1&& arg1, std::false_type, ArgN&&... argn);

    void
    on_request_impl(
        verb method,
        string_view method_str,
        string_view target,
        int version,
        error_code& ec)
    {
        try
        {
            m_.target(target);
            if(method != verb::unknown)
                m_.method(method);
            else
                m_.method_string(method_str);
            ec.assign(0, ec.category());
        }
        catch(std::bad_alloc const&)
        {
            ec = error::bad_alloc;
        }
        m_.version(version);
    }

    void
    on_response_impl(
        int code,
        string_view reason,
        int version,
        error_code& ec)
    {
        m_.result(code);
        m_.version(version);
        try
        {
            m_.reason(reason);
            ec.assign(0, ec.category());
        }
        catch(std::bad_alloc const&)
        {
            ec = error::bad_alloc;
        }
    }

    void
    on_field_impl(
        field name,
        string_view name_string,
        string_view value,
        error_code& ec)
    {
        try
        {
            m_.insert(name, name_string, value);
            ec.assign(0, ec.category());
        }
        catch(std::bad_alloc const&)
        {
            ec = error::bad_alloc;
        }
    }

    void
    on_header_impl(error_code& ec)
    {
        ec.assign(0, ec.category());
    }

    void
    on_body_init_impl(
        boost::optional<std::uint64_t> const& content_length,
        error_code& ec)
    {
        rd_.init(content_length, ec);
        rd_inited_ = true;
    }

    std::size_t
    on_body_impl(
        string_view body,
        error_code& ec)
    {
        return rd_.put(boost::asio::buffer(
            body.data(), body.size()), ec);
    }

    void
    on_chunk_header_impl(
        std::uint64_t size,
        string_view extensions,
        error_code& ec)
    {
        if(cb_h_)
            return cb_h_(size, extensions, ec);
        ec.assign(0, ec.category());
    }

    std::size_t
    on_chunk_body_impl(
        std::uint64_t remain,
        string_view body,
        error_code& ec)
    {
        if(cb_b_)
            return cb_b_(remain, body, ec);
        return rd_.put(boost::asio::buffer(
            body.data(), body.size()), ec);
    }

    void
    on_finish_impl(error_code& ec)
    {
        rd_.finish(ec);
    }
};

/// An HTTP/1 parser for producing a request message.
template<class Body, class Allocator = std::allocator<char>>
using request_parser = parser<true, Body, Allocator>;

/// An HTTP/1 parser for producing a response message.
template<class Body, class Allocator = std::allocator<char>>
using response_parser = parser<false, Body, Allocator>;

} // http
} // beast
} // boost

#include <boost/beast/http/impl/parser.ipp>

#endif
