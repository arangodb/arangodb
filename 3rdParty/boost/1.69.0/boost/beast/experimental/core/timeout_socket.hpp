//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_TIMEOUT_SOCKET_HPP
#define BOOST_BEAST_CORE_TIMEOUT_SOCKET_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/experimental/core/detail/timeout_service.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/executor.hpp>
#include <chrono>

namespace boost {
namespace asio {
namespace ip {
class tcp;
} // ip
} // asio
} // boost

namespace boost {
namespace beast {

/** A socket wrapper which automatically times out on asynchronous reads.

    This wraps a normal stream socket and implements a simple and efficient
    timeout for asynchronous read operations.

    @note Meets the requirements of @b AsyncReadStream and @b AsyncWriteStream
*/
template<
    class Protocol,
    class Executor = boost::asio::executor
>
class basic_timeout_socket
{
    template<class> class async_op;

    class timer : public detail::timeout_object
    {
        basic_timeout_socket& s_;

    public:
        explicit timer(basic_timeout_socket& s);
        timer& operator=(timer&& other);
        void on_timeout() override;
    };

    Executor ex_; // must come first
    timer rd_timer_;
    timer wr_timer_;
    boost::asio::basic_stream_socket<Protocol> sock_;
    bool expired_ = false;

public:
    /// The type of the next layer.
    using next_layer_type = boost::asio::basic_stream_socket<Protocol>;

    /// The type of the lowest layer.
    using lowest_layer_type = get_lowest_layer<next_layer_type>;

    /// The protocol used by the stream.
    using protocol_type = Protocol;

    /// The type of the executor associated with the object.
    using executor_type = Executor;

    // VFALCO we only support default-construction
    //        of the contained socket for now.
    //        This constructor needs a protocol parameter.
    //
    /** Constructor
    */
    template<class ExecutionContext
#if ! BOOST_BEAST_DOXYGEN
        , class = typename std::enable_if<
        std::is_convertible<
            ExecutionContext&,
            boost::asio::execution_context&>::value &&
        std::is_constructible<
            executor_type,
            typename ExecutionContext::executor_type>::value
        >::type
#endif
    >
    explicit
    basic_timeout_socket(ExecutionContext& ctx);
 
    //--------------------------------------------------------------------------

    /** Get the executor associated with the object.
    
        This function may be used to obtain the executor object that the
        stream uses to dispatch handlers for asynchronous operations.

        @return A copy of the executor that stream will use to dispatch handlers.
    */
    executor_type
    get_executor() const noexcept
    {
        return ex_;
    }

    /** Get a reference to the next layer

        This function returns a reference to the next layer
        in a stack of stream layers.

        @return A reference to the next layer in the stack of
        stream layers.
    */
    next_layer_type&
    next_layer()
    {
        return sock_;
    }

    /** Get a reference to the next layer

        This function returns a reference to the next layer in a
        stack of stream layers.

        @return A reference to the next layer in the stack of
        stream layers.
    */
    next_layer_type const&
    next_layer() const
    {
        return sock_;
    }

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers.
    */
    lowest_layer_type&
    lowest_layer()
    {
        return sock_.lowest_layer();
    }

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers. Ownership is not transferred to the caller.
    */
    lowest_layer_type const&
    lowest_layer() const
    {
        return sock_.lowest_layer();
    }

    //--------------------------------------------------------------------------

    /** Start an asynchronous read.

        This function is used to asynchronously read data from the stream socket.
        The function call always returns immediately.
        
        @param buffers One or more buffers into which the data will be read.
        Although the buffers object may be copied as necessary, ownership of the
        underlying memory blocks is retained by the caller, which must guarantee
        that they remain valid until the handler is called.
        
        @param handler The handler to be called when the read operation completes.
        Copies will be made of the handler as required. The function signature of
        the handler must be:
        @code void handler(
          const boost::system::error_code& error, // Result of operation.
          std::size_t bytes_transferred           // Number of bytes read.
        ); @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        boost::asio::io_context::post().
    */
    template<class MutableBufferSequence, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
        void(boost::system::error_code, std::size_t))
    async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler);

    /** Start an asynchronous write.

        This function is used to asynchronously write data to the stream socket.
        The function call always returns immediately.
        
        @param buffers One or more data buffers to be written to the socket.
        Although the buffers object may be copied as necessary, ownership of the
        underlying memory blocks is retained by the caller, which must guarantee
        that they remain valid until the handler is called.
        
        @param handler The handler to be called when the write operation completes.
        Copies will be made of the handler as required. The function signature of
        the handler must be:
        @code void handler(
          const boost::system::error_code& error, // Result of operation.
          std::size_t bytes_transferred           // Number of bytes written.
        ); @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        boost::asio::io_context::post().
    */
    template<class ConstBufferSequence, class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
        void(boost::system::error_code, std::size_t))
    async_write_some(
        ConstBufferSequence const& buffers,
        WriteHandler&& handler);
};

/// A TCP/IP socket wrapper which has a built-in asynchronous timeout
using timeout_socket = basic_timeout_socket<
    boost::asio::ip::tcp,
    boost::asio::io_context::executor_type>;

} // beast
} // boost

#include <boost/beast/experimental/core/impl/timeout_socket.hpp>

#endif
