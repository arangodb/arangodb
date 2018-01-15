//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_VECTOR_BODY_HPP
#define BOOST_BEAST_HTTP_VECTOR_BODY_HPP

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

/** A @b Body using `std::vector`

    This body uses `std::vector` as a memory-based container
    for holding message payloads. Messages using this body type
    may be serialized and parsed.
*/
template<class T, class Allocator = std::allocator<T>>
struct vector_body
{
private:
    static_assert(sizeof(T) == 1 &&
        std::is_integral<T>::value,
        "T requirements not met");

public:
    /** The type of container used for the body

        This determines the type of @ref message::body
        when this body type is used with a message container.
    */
    using value_type = std::vector<T, Allocator>;

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
        reader(message<isRequest,
                vector_body, Fields>& m)
            : body_(m.body())
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
            auto const n = buffer_size(buffers);
            auto const len = body_.size();
            try
            {
                body_.resize(len + n);
            }
            catch(std::exception const&)
            {
                ec = error::buffer_overflow;
                return 0;
            }
            ec.assign(0, ec.category());
            return buffer_copy(boost::asio::buffer(
                &body_[0] + len, n), buffers);
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
        writer(message<isRequest,
                vector_body, Fields> const& msg)
            : body_(msg.body())
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

} // http
} // beast
} // boost

#endif
