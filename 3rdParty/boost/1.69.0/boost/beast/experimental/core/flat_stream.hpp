//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_FLAT_STREAM_HPP
#define BOOST_BEAST_CORE_FLAT_STREAM_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/experimental/core/detail/flat_stream.hpp>
#include <boost/asio/async_result.hpp>
#include <cstdlib>
#include <utility>

namespace boost {
namespace beast {

/** Stream wrapper to improve ssl::stream write performance.

    This wrapper flattens writes for buffer sequences having length
    greater than 1 and total size below a predefined amount, using
    a dynamic memory allocation. It is primarily designed to overcome
    a performance limitation of the current version of `boost::asio::ssl::stream`,
    which does not use OpenSSL's scatter/gather interface for its
    low-level read some and write some operations.

    @par Example

    To use the @ref flat_stream template with SSL streams, declare
    a variable of the correct type. Parameters passed to the constructor
    will be forwarded to the next layer's constructor:

    @code
        flat_stream<ssl::stream<ip::tcp::socket>> fs{ioc, ctx};
    @endcode
    Alternatively you can write
    @code
        ssl::stream<ip::tcp::socket> ss{ioc, ctx};
        flat_stream<ssl::stream<ip::tcp::socket>&> fs{ss};
    @endcode

    The resulting stream may be passed to any stream algorithms which
    operate on synchronous or asynchronous read or write streams,
    examples include:
    
    @li `boost::asio::read`, `boost::asio::async_read`

    @li `boost::asio::write`, `boost::asio::async_write`

    @li `boost::asio::read_until`, `boost::asio::async_read_until`

    The stream may also be used as a template parameter in other
    stream wrappers, such as for websocket:
    @code
        websocket::stream<flat_stream<ssl::stream<ip::tcp::socket>>> ws{ioc, ctx};
    @endcode

    @tparam NextLayer The type representing the next layer, to which
    data will be read and written during operations. For synchronous
    operations, the type must support the @b SyncStream concept. For
    asynchronous operations, the type must support the @b AsyncStream
    concept. This type will usually be some variation of
    `boost::asio::ssl::stream`.

    @par Concepts
        @b AsyncStream
        @b SyncStream

    @see
        @li https://github.com/boostorg/beast/issues/1108
        @li https://github.com/boostorg/asio/issues/100
        @li https://stackoverflow.com/questions/38198638/openssl-ssl-write-from-multiple-buffers-ssl-writev
        @li https://stackoverflow.com/questions/50026167/performance-drop-on-port-from-beast-1-0-0-b66-to-boost-1-67-0-beast
*/
template<class NextLayer>
class flat_stream
#if ! BOOST_BEAST_DOXYGEN
    : private detail::flat_stream_base
#endif
{
    // Largest buffer size we will flatten.
    // 16KB is the upper limit on reasonably sized HTTP messages.
    static std::size_t constexpr max_size = 16 * 1024;

    template<class, class> class write_op;

    NextLayer stream_;

public:
    /// The type of the next layer.
    using next_layer_type =
        typename std::remove_reference<NextLayer>::type;

    /// The type of the lowest layer.
    using lowest_layer_type = boost::beast::get_lowest_layer<next_layer_type>;

    /// The type of the executor associated with the object.
    using executor_type = typename next_layer_type::executor_type;

    flat_stream(flat_stream&&) = default;
    flat_stream(flat_stream const&) = default;
    flat_stream& operator=(flat_stream&&) = default;
    flat_stream& operator=(flat_stream const&) = default;

    /** Destructor

        The treatment of pending operations will be the same as that
        of the next layer.
    */
    ~flat_stream() = default;

    /** Constructor

        Arguments, if any, are forwarded to the next layer's constructor.
    */
    template<class... Args>
    explicit
    flat_stream(Args&&... args);

    //--------------------------------------------------------------------------

    /** Get the executor associated with the object.
    
        This function may be used to obtain the executor object that the
        stream uses to dispatch handlers for asynchronous operations.

        @return A copy of the executor that stream will use to dispatch handlers.
    */
    executor_type
    get_executor() noexcept
    {
        return stream_.get_executor();
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
        return stream_;
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
        return stream_;
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
        return stream_.lowest_layer();
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
        return stream_.lowest_layer();
    }

    //--------------------------------------------------------------------------

    /** Read some data from the stream.

        This function is used to read data from the stream. The function call will
        block until one or more bytes of data has been read successfully, or until
        an error occurs.
        
        @param buffers The buffers into which the data will be read.
        
        @returns The number of bytes read.
        
        @throws boost::system::system_error Thrown on failure.
        
        @note The `read_some` operation may not read all of the requested number of
        bytes. Consider using the function `boost::asio::read` if you need to ensure
        that the requested amount of data is read before the blocking operation
        completes.
    */
    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers);

    /** Read some data from the stream.

        This function is used to read data from the stream. The function call will
        block until one or more bytes of data has been read successfully, or until
        an error occurs.
        
        @param buffers The buffers into which the data will be read.
        
        @param ec Set to indicate what error occurred, if any.

        @returns The number of bytes read.
                
        @note The `read_some` operation may not read all of the requested number of
        bytes. Consider using the function `boost::asio::read` if you need to ensure
        that the requested amount of data is read before the blocking operation
        completes.
    */
    template<class MutableBufferSequence>
    std::size_t
    read_some(
        MutableBufferSequence const& buffers,
        error_code& ec);

    /** Start an asynchronous read.
    
        This function is used to asynchronously read one or more bytes of data from
        the stream. The function call always returns immediately.
        
        @param buffers The buffers into which the data will be read. Although the
        buffers object may be copied as necessary, ownership of the underlying
        buffers is retained by the caller, which must guarantee that they remain
        valid until the handler is called.
        
        @param handler The handler to be called when the read operation completes.
        Copies will be made of the handler as required. The equivalent function
        signature of the handler must be:
        @code void handler(
          const boost::system::error_code& error, // Result of operation.
          std::size_t bytes_transferred           // Number of bytes read.
        ); @endcode
        
        @note The `read_some` operation may not read all of the requested number of
        bytes. Consider using the function `boost::asio::async_read` if you need
        to ensure that the requested amount of data is read before the asynchronous
        operation completes.
    */
    template<
        class MutableBufferSequence,
        class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(
        ReadHandler, void(error_code, std::size_t))
    async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler);

    /** Write some data to the stream.
    
        This function is used to write data on the stream. The function call will
        block until one or more bytes of data has been written successfully, or
        until an error occurs.
        
        @param buffers The data to be written.
        
        @returns The number of bytes written.
        
        @throws boost::system::system_error Thrown on failure.
        
        @note The `write_some` operation may not transmit all of the data to the
        peer. Consider using the function `boost::asio::write` if you need to
        ensure that all data is written before the blocking operation completes.
    */
    template<class ConstBufferSequence>
    std::size_t
    write_some(ConstBufferSequence const& buffers);

    /** Write some data to the stream.
    
        This function is used to write data on the stream. The function call will
        block until one or more bytes of data has been written successfully, or
        until an error occurs.
        
        @param buffers The data to be written.
        
        @param ec Set to indicate what error occurred, if any.

        @returns The number of bytes written.
                
        @note The `write_some` operation may not transmit all of the data to the
        peer. Consider using the function `boost::asio::write` if you need to
        ensure that all data is written before the blocking operation completes.
    */
    template<class ConstBufferSequence>
    std::size_t
    write_some(
        ConstBufferSequence const& buffers,
        error_code& ec);

    /** Start an asynchronous write.
        
        This function is used to asynchronously write one or more bytes of data to
        the stream. The function call always returns immediately.
        
        @param buffers The data to be written to the stream. Although the buffers
        object may be copied as necessary, ownership of the underlying buffers is
        retained by the caller, which must guarantee that they remain valid until
        the handler is called.
        
        @param handler The handler to be called when the write operation completes.
        Copies will be made of the handler as required. The equivalent function
        signature of the handler must be:
        @code void handler(
          const boost::system::error_code& error, // Result of operation.
          std::size_t bytes_transferred           // Number of bytes written.
        ); @endcode
        
        @note The `async_write_some` operation may not transmit all of the data to
        the peer. Consider using the function `boost::asio::async_write` if you need
        to ensure that all data is written before the asynchronous operation completes.
    */
    template<
        class ConstBufferSequence,
        class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(
        WriteHandler, void(error_code, std::size_t))
    async_write_some(
        ConstBufferSequence const& buffers,
        WriteHandler&& handler);
};

} // beast
} // boost

#include <boost/beast/experimental/core/impl/flat_stream.ipp>

#endif
