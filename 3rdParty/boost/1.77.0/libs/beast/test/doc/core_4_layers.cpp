//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "snippets.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/async_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <utility>

namespace boost {
namespace beast {

void
core_4_layers_snippets()
{
    #include "snippets.ipp"
    {
    //[code_core_4_layers_1

        net::ssl::stream<net::ip::tcp::socket> ss(ioc, ctx);

    //]
    }
    {
    //[code_core_4_layers_2

        websocket::stream<net::ip::tcp::socket> ws(ioc);

    //]
    }
    //[code_core_4_layers_3

        websocket::stream<net::ssl::stream<net::ip::tcp::socket>> ws(ioc, ctx);

    //]
}

//[code_core_4_layers_4

// Set non-blocking mode on a stack of stream
// layers with a regular socket at the lowest layer.
template <class Stream>
void set_non_blocking (Stream& stream)
{
    error_code ec;
    // A compile error here means your lowest layer is not the right type!
    get_lowest_layer(stream).non_blocking(true, ec);
    if(ec)
        throw system_error{ec};
}

//]

//[code_core_4_layers_5

// A layered stream which counts the bytes read and bytes written on the next layer
template <class NextLayer>
class counted_stream
{
    NextLayer next_layer_;              // Reads and writes are passed through to this
    std::size_t bytes_read_ = 0;        // Holds the total bytes read
    std::size_t bytes_written_ = 0;     // Holds the total bytes written

    // This is the "initiation" object passed to async_initiate to start the operation
    struct run_read_op
    {
        template<
            class ReadHandler,
            class MutableBufferSequence>
        void
        operator()(
            ReadHandler&& handler,
            counted_stream* stream,
            MutableBufferSequence const& buffers)
        {
            using handler_type = typename std::decay<ReadHandler>::type;

            // async_base handles all of the composed operation boilerplate for us
            using base = async_base<
                handler_type, beast::executor_type<NextLayer>>;

            // Our composed operation is implemented as a completion handler object
            struct op : base
            {
                counted_stream& stream_;

                op( counted_stream& stream,
                    handler_type&& handler,
                    MutableBufferSequence const& buffers)
                    : base(std::move(handler), stream.get_executor())
                    , stream_(stream)
                {
                    // Start the asynchronous operation
                    stream_.next_layer().async_read_some(buffers, std::move(*this));
                }

                void operator()(error_code ec, std::size_t bytes_transferred)
                {
                    // Count the bytes transferred towards the total
                    stream_.bytes_read_ += bytes_transferred;

                    this->complete_now(ec, bytes_transferred);
                }
            };

            op(*stream, std::forward<ReadHandler>(handler), buffers);
        }
    };

    // This is the "initiation" object passed to async_initiate to start the operation
    struct run_write_op
    {
        template<
            class WriteHandler,
            class ConstBufferSequence>
        void
        operator()(
            WriteHandler&& handler,
            counted_stream* stream,
            ConstBufferSequence const& buffers)
        {
            using handler_type = typename std::decay<WriteHandler>::type;

            // async_base handles all of the composed operation boilerplate for us
            using base = async_base<
                handler_type, beast::executor_type<NextLayer>>;

            // Our composed operation is implemented as a completion handler object
            struct op : base
            {
                counted_stream& stream_;

                op( counted_stream& stream,
                    handler_type&& handler,
                    ConstBufferSequence const& buffers)
                    : base(std::move(handler), stream.get_executor())
                    , stream_(stream)
                {
                    // Start the asynchronous operation
                    stream_.next_layer().async_write_some(buffers, std::move(*this));
                }

                void operator()(error_code ec, std::size_t bytes_transferred)
                {
                    // Count the bytes transferred towards the total
                    stream_.bytes_written_ += bytes_transferred;

                    this->complete_now(ec, bytes_transferred);
                }
            };

            op(*stream, std::forward<WriteHandler>(handler), buffers);
        }
    };

public:
    /// The type of executor used by this stream
    using executor_type = beast::executor_type<NextLayer>;

    /// Constructor
    template <class... Args>
    explicit
    counted_stream(Args&&... args)
        : next_layer_(std::forward<Args>(args)...)
    {
    }

    /// Returns an instance of the executor used to submit completion handlers
    executor_type get_executor() noexcept
    {
        return next_layer_.get_executor();
    }

    /// Returns a reference to the next layer
    NextLayer& next_layer() noexcept
    {
        return next_layer_;
    }

    /// Returns a reference to the next layer
    NextLayer const& next_layer() const noexcept
    {
        return next_layer_;
    }

    /// Returns the total number of bytes read since the stream was constructed
    std::size_t bytes_read() const noexcept
    {
        return bytes_read_;
    }

    /// Returns the total number of bytes written since the stream was constructed
    std::size_t bytes_written() const noexcept
    {
        return bytes_written_;
    }

    /// Read some data from the stream
    template <class MutableBufferSequence>
    std::size_t read_some(MutableBufferSequence const& buffers)
    {
        auto const bytes_transferred = next_layer_.read_some(buffers);
        bytes_read_ += bytes_transferred;
        return bytes_transferred;
    }

    /// Read some data from the stream
    template <class MutableBufferSequence>
    std::size_t read_some(MutableBufferSequence const& buffers, error_code& ec)
    {
        auto const bytes_transferred = next_layer_.read_some(buffers, ec);
        bytes_read_ += bytes_transferred;
        return bytes_transferred;
    }

    /// Write some data to the stream
    template <class ConstBufferSequence>
    std::size_t write_some(ConstBufferSequence const& buffers)
    {
        auto const bytes_transferred = next_layer_.write_some(buffers);
        bytes_written_ += bytes_transferred;
        return bytes_transferred;
    }

    /// Write some data to the stream
    template <class ConstBufferSequence>
    std::size_t write_some(ConstBufferSequence const& buffers, error_code& ec)
    {
        auto const bytes_transferred = next_layer_.write_some(buffers, ec);
        bytes_written_ += bytes_transferred;
        return bytes_transferred;
    }

    /// Read some data from the stream asynchronously
    template<
        class MutableBufferSequence,
        class ReadHandler =
            net::default_completion_token_t<executor_type>>
    BOOST_BEAST_ASYNC_RESULT2(ReadHandler)
    async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler =
            net::default_completion_token_t<executor_type>{})
    {
        return net::async_initiate<
            ReadHandler,
            void(error_code, std::size_t)>(
                run_read_op{},
                handler,
                this,
                buffers);
    }

    /// Write some data to the stream asynchronously
    template<
        class ConstBufferSequence,
        class WriteHandler =
            net::default_completion_token_t<executor_type>>
    BOOST_BEAST_ASYNC_RESULT2(WriteHandler)
    async_write_some(
        ConstBufferSequence const& buffers,
        WriteHandler&& handler =
            net::default_completion_token_t<
                executor_type>{})
    {
        return net::async_initiate<
            WriteHandler,
            void(error_code, std::size_t)>(
                run_write_op{},
                handler,
                this,
                buffers);
    }
};
//]

template class counted_stream<test::stream>;

BOOST_STATIC_ASSERT(is_sync_read_stream<counted_stream<test::stream>>::value);
BOOST_STATIC_ASSERT(is_sync_write_stream<counted_stream<test::stream>>::value);
BOOST_STATIC_ASSERT(is_async_read_stream<counted_stream<test::stream>>::value);
BOOST_STATIC_ASSERT(is_async_write_stream<counted_stream<test::stream>>::value);

struct core_4_layers_test
    : public beast::unit_test::suite
{
    struct handler
    {
        void operator()(error_code, std::size_t)
        {
        }
    };

    void
    run() override
    {
        BEAST_EXPECT(&core_4_layers_snippets);
        BEAST_EXPECT(&set_non_blocking<net::ip::tcp::socket>);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,core_4_layers);

} // beast
} // boost
