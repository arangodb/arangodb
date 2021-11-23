//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/stream_traits.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <utility>

namespace boost {
namespace beast {

class stream_traits_test
    : public beast::unit_test::suite
{
public:
    struct without
    {
        int dummy = 0;

        without() = default;

        template<class T>
        std::size_t write_some(T const&)
        {
            return 0;
        }

        template<class T>
        std::size_t write_some(T const&, boost::system::error_code&)
        {
            return 0;
        }
    };

    template<class T>
    struct with
    {
        T t;

        with() = default;

        T&
        next_layer()
        {
            return t;
        }

        T const&
        next_layer() const
        {
            return t;
        }
    };

    BOOST_STATIC_ASSERT(
        ! detail::has_next_layer<without>::value);

    BOOST_STATIC_ASSERT(
        detail::has_next_layer<with<without>>::value);

    BOOST_STATIC_ASSERT(
        detail::has_next_layer<with<with<without>>>::value);

    void
    testGetLowestLayer()
    {
        {
            without w{};
            BEAST_EXPECT(&get_lowest_layer(w) == &w);
        }
        {
            without const w{};
            BEAST_EXPECT(&get_lowest_layer(w) == &w);
        }
        {
            with<without> w{};
            BEAST_EXPECT(&get_lowest_layer(w) == &w.t);
        }
        {
            with<without> const w{};
            BEAST_EXPECT(&get_lowest_layer(w) == &w.t);
        }
        {
            with<with<without>> w{};
            BEAST_EXPECT(&get_lowest_layer(w) == &w.t.t);
        }
        {
            with<with<without>> const w{};
            BEAST_EXPECT(&get_lowest_layer(w) == &w.t.t);
        }
        {
            with<with<with<without>>> w{};
            BEAST_EXPECT(&get_lowest_layer(w) == &w.t.t.t);
        }
        {
            with<with<with<without>>> const w{};
            BEAST_EXPECT(&get_lowest_layer(w) == &w.t.t.t);
        }
    }

    //--------------------------------------------------------------------------
/*
    @par Example
    This code implements a <em>SyncWriteStream</em> wrapper which calls
    `std::terminate` upon any error.
*/
    template <class NextLayer>
    class write_stream
    {
        NextLayer next_layer_;

    public:
        static_assert(is_sync_write_stream<NextLayer>::value,
            "SyncWriteStream type requirements not met");

        template<class... Args>
        explicit
        write_stream(Args&&... args)
            : next_layer_(std::forward<Args>(args)...)
        {
        }

        NextLayer& next_layer() noexcept
        {
            return next_layer_;
        }

        NextLayer const& next_layer() const noexcept
        {
            return next_layer_;
        }

        template<class ConstBufferSequence>
        std::size_t
        write_some(ConstBufferSequence const& buffers)
        {
            error_code ec;
            auto const bytes_transferred = next_layer_.write_some(buffers, ec);
            if(ec)
                std::terminate();
            return bytes_transferred;
        }

        template<class ConstBufferSequence>
        std::size_t
        write_some(ConstBufferSequence const& buffers, error_code& ec)
        {
            auto const bytes_transferred = next_layer_.write_some(buffers, ec);
            if(ec)
                std::terminate();
            return bytes_transferred;
        }
    };
    
    void
    testGetLowestLayerJavadoc()
    {
        write_stream<without> s;
        BOOST_STATIC_ASSERT(
            is_sync_write_stream<without>::value);
        BOOST_STATIC_ASSERT(std::is_same<
            decltype(get_lowest_layer(s)), without&>::value);

#if 0
        BEAST_EXPECT(static_cast<
            std::size_t(type::*)(net::const_buffer)>(
                &type::write_some<net::const_buffer>));
#endif
    }

    //--------------------------------------------------------------------------

    void
    testExecutorType()
    {
    }

    void
    testExecutorTypeJavadoc()
    {
    }

    //--------------------------------------------------------------------------

    struct sync_read_stream
    {
        template<class MutableBufferSequence>
        std::size_t read_some(MutableBufferSequence const&);
        template<class MutableBufferSequence>
        std::size_t read_some(MutableBufferSequence const&, error_code& ec);
    };

    struct sync_write_stream
    {
        template<class ConstBufferSequence>
        std::size_t write_some(ConstBufferSequence const&);
        template<class ConstBufferSequence>
        std::size_t write_some(ConstBufferSequence const&, error_code&);
    };

    struct async_read_stream
    {
        net::io_context::executor_type get_executor() noexcept;
        template<class MutableBufferSequence, BOOST_BEAST_ASYNC_TPARAM2 ReadHandler>
        void async_read_some(MutableBufferSequence const&, ReadHandler&&);
    };

    struct async_write_stream
    {
        net::io_context::executor_type get_executor() noexcept;
        template<class ConstBufferSequence, BOOST_BEAST_ASYNC_TPARAM2 WriteHandler>
        void async_write_some(ConstBufferSequence const&, WriteHandler&&);
    };

    struct sync_stream : sync_read_stream, sync_write_stream
    {
    };

    struct async_stream : async_read_stream, async_write_stream
    {
        net::io_context::executor_type get_executor() noexcept;
        template<class MutableBufferSequence, BOOST_BEAST_ASYNC_TPARAM2 ReadHandler>
        void async_read_some(MutableBufferSequence const&, ReadHandler&&);
        template<class ConstBufferSequence, BOOST_BEAST_ASYNC_TPARAM2 WriteHandler>
        void async_write_some(ConstBufferSequence const&, WriteHandler&&);
    };

    BOOST_STATIC_ASSERT(is_sync_read_stream<sync_read_stream>::value);
    BOOST_STATIC_ASSERT(is_sync_read_stream<sync_stream>::value);
    BOOST_STATIC_ASSERT(is_sync_write_stream<sync_write_stream>::value);
    BOOST_STATIC_ASSERT(is_sync_write_stream<sync_stream>::value);
    BOOST_STATIC_ASSERT(is_sync_stream<sync_stream>::value);

    BOOST_STATIC_ASSERT(! is_sync_read_stream<sync_write_stream>::value);
    BOOST_STATIC_ASSERT(! is_sync_write_stream<sync_read_stream>::value);
    BOOST_STATIC_ASSERT(! is_sync_stream<async_stream>::value);

    BOOST_STATIC_ASSERT(has_get_executor<async_read_stream>::value);
    BOOST_STATIC_ASSERT(has_get_executor<async_write_stream>::value);
    BOOST_STATIC_ASSERT(has_get_executor<async_stream>::value);

    BOOST_STATIC_ASSERT(! has_get_executor<sync_read_stream>::value);
    BOOST_STATIC_ASSERT(! has_get_executor<sync_write_stream>::value);
    BOOST_STATIC_ASSERT(! has_get_executor<sync_stream>::value);

    BOOST_STATIC_ASSERT(is_async_read_stream<async_read_stream>::value);
    BOOST_STATIC_ASSERT(is_async_read_stream<async_stream>::value);
#if BOOST_WORKAROUND(BOOST_MSVC, < 1910)
    BOOST_STATIC_ASSERT(is_async_write_stream<net::ip::tcp::socket>::value);
#else
    BOOST_STATIC_ASSERT(is_async_write_stream<async_write_stream>::value);
#endif
    BOOST_STATIC_ASSERT(is_async_write_stream<async_stream>::value);
    BOOST_STATIC_ASSERT(is_async_stream<async_stream>::value);

    BOOST_STATIC_ASSERT(! is_async_write_stream<async_read_stream>::value);
    BOOST_STATIC_ASSERT(! is_async_read_stream<async_write_stream>::value);
    BOOST_STATIC_ASSERT(! is_async_stream<sync_stream>::value);

    //--------------------------------------------------------------------------

    template<class T>
    struct layer
    {
        T t;

        template<class U>
        explicit
        layer(U&& u)
            : t(std::forward<U>(u))
        {
        }

        T& next_layer()
        {
            return t;
        }
    };

    void
    testClose()
    {
        net::io_context ioc;
        {
            net::ip::tcp::socket sock(ioc);
            sock.open(net::ip::tcp::v4());
            BEAST_EXPECT(sock.is_open());
            close_socket(get_lowest_layer(sock));
            BEAST_EXPECT(! sock.is_open());
        }
        {
            using type = layer<net::ip::tcp::socket>;
            type layer(ioc);
            layer.next_layer().open(net::ip::tcp::v4());
            BEAST_EXPECT(layer.next_layer().is_open());
            BOOST_STATIC_ASSERT(detail::has_next_layer<type>::value);
            BOOST_STATIC_ASSERT((std::is_same<
                typename std::decay<decltype(get_lowest_layer(layer))>::type,
                lowest_layer_type<decltype(layer)>>::value));
            BOOST_STATIC_ASSERT(std::is_same<net::ip::tcp::socket&,
                decltype(get_lowest_layer(layer))>::value);
            BOOST_STATIC_ASSERT(std::is_same<net::ip::tcp::socket,
                lowest_layer_type<decltype(layer)>>::value);
            close_socket(get_lowest_layer(layer));
            BEAST_EXPECT(! layer.next_layer().is_open());
        }
        {
            test::stream ts(ioc);
            close_socket(ts);
        }
    }

    //--------------------------------------------------------------------------

    template <class WriteStream>
    void hello_and_close (WriteStream& stream)
    {
        net::write(stream, net::const_buffer("Hello, world!", 13));
        close_socket(get_lowest_layer(stream));
    }

    class my_socket
    {
        net::ip::tcp::socket sock_;

    public:
        my_socket(net::io_context& ioc)
            : sock_(ioc)
        {
        }

        friend void beast_close_socket(my_socket& s)
        {
            error_code ec;
            s.sock_.close(ec);
            // ignore the error
        }
    };

    void
    testCloseJavadoc()
    {
        BEAST_EXPECT(&stream_traits_test::template hello_and_close<net::ip::tcp::socket>);
        {
            net::io_context ioc;
            my_socket s(ioc);
            close_socket(s);
        }
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        testGetLowestLayer();
        testGetLowestLayerJavadoc();
        testExecutorType();
        testExecutorTypeJavadoc();
        testClose();
        testCloseJavadoc();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,stream_traits);

} // beast
} // boost
