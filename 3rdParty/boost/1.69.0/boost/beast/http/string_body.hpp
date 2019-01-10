//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_STRING_BODY_HPP
#define BOOST_BEAST_HTTP_STRING_BODY_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace boost {
namespace beast {
namespace http {

/** A @b Body using `std::basic_string`

    This body uses `std::basic_string` as a memory-based container
    for holding message payloads. Messages using this body type
    may be serialized and parsed.
*/
template<
    class CharT,
    class Traits = std::char_traits<CharT>,
    class Allocator = std::allocator<CharT>>
struct basic_string_body
{
private:
    static_assert(
        std::is_integral<CharT>::value &&
            sizeof(CharT) == 1,
        "CharT requirements not met");

public:
    /** The type of container used for the body

        This determines the type of @ref message::body
        when this body type is used with a message container.
    */
    using value_type =
        std::basic_string<CharT, Traits, Allocator>;

    /** Returns the payload size of the body

        When this body is used with @ref message::prepare_payload,
        the Content-Length will be set to the payload size, and
        any chunked Transfer-Encoding will be removed.
    */
    static
    std::uint64_t
    size(value_type const& body)
    {
        return body.size();
    }

    /** The algorithm for parsing the body

        Meets the requirements of @b BodyReader.
    */
#if BOOST_BEAST_DOXYGEN
    using reader = implementation_defined;
#else
    class reader
    {
        value_type& body_;

    public:
        template<bool isRequest, class Fields>
        explicit
        reader(header<isRequest, Fields>&, value_type& b)
            : body_(b)
        {
        }

        void
        init(boost::optional<
            std::uint64_t> const& length, error_code& ec)
        {
            if(length)
            {
                if(static_cast<std::size_t>(*length) != *length)
                {
                    ec = error::buffer_overflow;
                    return;
                }
                try
                {
                    body_.reserve(
                        static_cast<std::size_t>(*length));
                }
                catch(std::exception const&)
                {
                    ec = error::buffer_overflow;
                    return;
                }
            }
            ec.assign(0, ec.category());
        }

        template<class ConstBufferSequence>
        std::size_t
        put(ConstBufferSequence const& buffers,
            error_code& ec)
        {
            using boost::asio::buffer_size;
            using boost::asio::buffer_copy;
            auto const extra = buffer_size(buffers);
            auto const size = body_.size();
            try
            {
                body_.resize(size + extra);
            }
            catch(std::exception const&)
            {
                ec = error::buffer_overflow;
                return 0;
            }
            ec.assign(0, ec.category());
            CharT* dest = &body_[size];
            for(auto b : beast::detail::buffers_range(buffers))
            {
                Traits::copy(dest, static_cast<
                    CharT const*>(b.data()), b.size());
                dest += b.size();
            }
            return extra;
        }

        void
        finish(error_code& ec)
        {
            ec.assign(0, ec.category());
        }
    };
#endif

    /** The algorithm for serializing the body

        Meets the requirements of @b BodyWriter.
    */
#if BOOST_BEAST_DOXYGEN
    using writer = implementation_defined;
#else
    class writer
    {
        value_type const& body_;

    public:
        using const_buffers_type =
            boost::asio::const_buffer;

        template<bool isRequest, class Fields>
        explicit
        writer(header<isRequest, Fields> const&, value_type const& b)
            : body_(b)
        {
        }

        void
        init(error_code& ec)
        {
            ec.assign(0, ec.category());
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            ec.assign(0, ec.category());
            return {{const_buffers_type{
                body_.data(), body_.size()}, false}};
        }
    };
#endif
};

/// A @b Body using `std::string`
using string_body = basic_string_body<char>;

} // http
} // beast
} // boost

#endif
