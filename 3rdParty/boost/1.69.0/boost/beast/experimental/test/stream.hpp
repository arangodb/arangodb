//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_STREAM_HPP
#define BOOST_BEAST_TEST_STREAM_HPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/experimental/test/fail_count.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

namespace boost {
namespace beast {
namespace test {

/** A two-way socket useful for unit testing

    An instance of this class simulates a traditional socket,
    while also providing features useful for unit testing.
    Each endpoint maintains an independent buffer called
    the input area. Writes from one endpoint append data
    to the peer's pending input area. When an endpoint performs
    a read and data is present in the input area, the data is
    delivered to the blocking or asynchronous operation. Otherwise
    the operation is blocked or deferred until data is made
    available, or until the endpoints become disconnected.

    These streams may be used anywhere an algorithm accepts a
    reference to a synchronous or asynchronous read or write
    stream. It is possible to use a test stream in a call to
    `boost::asio::read_until`, or in a call to
    @ref boost::beast::http::async_write for example.

    As with Boost.Asio I/O objects, a @ref stream constructs
    with a reference to the `boost::asio::io_context` to use for
    handling asynchronous I/O. For asynchronous operations, the
    stream follows the same rules as a traditional asio socket
    with respect to how completion handlers for asynchronous
    operations are performed.

    To facilitate testing, these streams support some additional
    features:

    @li The input area, represented by a @ref flat_buffer, may
    be directly accessed by the caller to inspect the contents
    before or after the remote endpoint writes data. This allows
    a unit test to verify that the received data matches.

    @li Data may be manually appended to the input area. This data
    will delivered in the next call to
    @ref stream::read_some or @ref stream::async_read_some.
    This allows predefined test vectors to be set up for testing
    read algorithms.

    @li The stream may be constructed with a fail count. The
    stream will eventually fail with a predefined error after a
    certain number of operations, where the number of operations
    is controlled by the test. When a test loops over a range of
    operation counts, it is possible to exercise every possible
    point of failure in the algorithm being tested. When used
    correctly the technique allows the tests to reach a high
    percentage of code coverage.

    @par Thread Safety
        @e Distinct @e objects: Safe.@n
        @e Shared @e objects: Unsafe.
        The application must also ensure that all asynchronous
        operations are performed within the same implicit or explicit strand.

    @par Concepts
        @li @b SyncReadStream
        @li @b SyncWriteStream
        @li @b AsyncReadStream
        @li @b AsyncWriteStream
*/
class stream
{
    struct read_op_base
    {
        virtual ~read_op_base() = default;
        virtual void operator()() = 0;
    };

    template<class Handler, class Buffers>
    class read_op;

    enum class status
    {
        ok,
        eof,
        reset
    };

    struct state
    {
        friend class stream;

        std::mutex m;
        flat_buffer b;
        std::condition_variable cv;
        std::unique_ptr<read_op_base> op;
        boost::asio::io_context& ioc;
        status code = status::ok;
        fail_count* fc = nullptr;
        std::size_t nread = 0;
        std::size_t nwrite = 0;
        std::size_t read_max =
            (std::numeric_limits<std::size_t>::max)();
        std::size_t write_max =
            (std::numeric_limits<std::size_t>::max)();

        ~state()
        {
            BOOST_ASSERT(! op);
        }

        explicit
        state(
            boost::asio::io_context& ioc_,
            fail_count* fc_)
            : ioc(ioc_)
            , fc(fc_)
        {
        }

        void
        on_write()
        {
            if(op)
            {
                std::unique_ptr<read_op_base> op_ = std::move(op);
                op_->operator()();
            }
            else
            {
                cv.notify_all();
            }
        }
    };

    std::shared_ptr<state> in_;
    std::weak_ptr<state> out_;

public:
    using buffer_type = flat_buffer;

    /// The type of the lowest layer.
    using lowest_layer_type = stream;

    /** Destructor

        If an asynchronous read operation is pending, it will
        simply be discarded with no notification to the completion
        handler.

        If a connection is established while the stream is destroyed,
        the peer will see the error `boost::asio::error::connection_reset`
        when performing any reads or writes.
    */
    ~stream();

    /** Move Constructor

        Moving the stream while asynchronous operations are pending
        results in undefined behavior.
    */
    stream(stream&& other);

    /** Move Assignment

        Moving the stream while asynchronous operations are pending
        results in undefined behavior.
    */
    stream&
    operator=(stream&& other);

    /** Construct a stream

        The stream will be created in a disconnected state.

        @param ioc The `io_context` object that the stream will use to
        dispatch handlers for any asynchronous operations.
    */
    explicit
    stream(boost::asio::io_context& ioc);

    /** Construct a stream

        The stream will be created in a disconnected state.

        @param ioc The `io_context` object that the stream will use to
        dispatch handlers for any asynchronous operations.

        @param fc The @ref fail_count to associate with the stream.
        Each I/O operation performed on the stream will increment the
        fail count.  When the fail count reaches its internal limit,
        a simulated failure error will be raised.
    */
    stream(
        boost::asio::io_context& ioc,
        fail_count& fc);

    /** Construct a stream

        The stream will be created in a disconnected state.

        @param ioc The `io_context` object that the stream will use to
        dispatch handlers for any asynchronous operations.

        @param s A string which will be appended to the input area, not
        including the null terminator.
    */
    stream(
        boost::asio::io_context& ioc,
        string_view s);

    /** Construct a stream

        The stream will be created in a disconnected state.

        @param ioc The `io_context` object that the stream will use to
        dispatch handlers for any asynchronous operations.

        @param fc The @ref fail_count to associate with the stream.
        Each I/O operation performed on the stream will increment the
        fail count.  When the fail count reaches its internal limit,
        a simulated failure error will be raised.

        @param s A string which will be appended to the input area, not
        including the null terminator.
    */
    stream(
        boost::asio::io_context& ioc,
        fail_count& fc,
        string_view s);

    /// Establish a connection
    void
    connect(stream& remote);

    /// The type of the executor associated with the object.
    using executor_type =
        boost::asio::io_context::executor_type;

    /// Return the executor associated with the object.
    boost::asio::io_context::executor_type
    get_executor() noexcept
    {
        return in_->ioc.get_executor();
    };

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers.
    */
    lowest_layer_type&
    lowest_layer()
    {
        return *this;
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
        return *this;
    }

    /// Set the maximum number of bytes returned by read_some
    void
    read_size(std::size_t n)
    {
        in_->read_max = n;
    }

    /// Set the maximum number of bytes returned by write_some
    void
    write_size(std::size_t n)
    {
        in_->write_max = n;
    }

    /// Direct input buffer access
    buffer_type&
    buffer()
    {
        return in_->b;
    }

    /// Returns a string view representing the pending input data
    string_view
    str() const;

    /// Appends a string to the pending input data
    void
    append(string_view s);

    /// Clear the pending input area
    void
    clear();

    /// Return the number of reads
    std::size_t
    nread() const
    {
        return in_->nread;
    }

    /// Return the number of writes
    std::size_t
    nwrite() const
    {
        return in_->nwrite;
    }

    /** Close the stream.

        The other end of the connection will see
        `error::eof` after reading all the remaining data.
    */
    void
    close();

    /** Close the other end of the stream.

        This end of the connection will see
        `error::eof` after reading all the remaining data.
    */
    void
    close_remote();

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
    read_some(MutableBufferSequence const& buffers,
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
    template<class MutableBufferSequence, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(
        ReadHandler, void(error_code, std::size_t))
    async_read_some(MutableBufferSequence const& buffers,
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
        ConstBufferSequence const& buffers, error_code& ec);

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
    template<class ConstBufferSequence, class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(
        WriteHandler, void(error_code, std::size_t))
    async_write_some(ConstBufferSequence const& buffers,
        WriteHandler&& handler);

#if ! BOOST_BEAST_DOXYGEN
    friend
    void
    teardown(
        websocket::role_type,
        stream& s,
        boost::system::error_code& ec);

    template<class TeardownHandler>
    friend
    void
    async_teardown(
        websocket::role_type role,
        stream& s,
        TeardownHandler&& handler);
#endif
};

#if BOOST_BEAST_DOXYGEN
/** Return a new stream connected to the given stream

    @param to The stream to connect to.

    @param args Optional arguments forwarded to the new stream's constructor.

    @return The new, connected stream.
*/
template<class... Args>
stream
connect(stream& to, Args&&... args);

#else
stream
connect(stream& to);

template<class Arg1, class... ArgN>
stream
connect(stream& to, Arg1&& arg1, ArgN&&... argn);
#endif

} // test
} // beast
} // boost

#include <boost/beast/experimental/test/impl/stream.ipp>

#endif
