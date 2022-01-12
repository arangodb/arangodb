//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffered_read_stream.hpp>

#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>
#include <boost/optional.hpp>
#if BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/use_awaitable.hpp>
#endif

namespace boost {
namespace beast {

class buffered_read_stream_test
    : public unit_test::suite
    , public test::enable_yield_to
{
    using self = buffered_read_stream_test;

public:
    void testSpecialMembers()
    {
        net::io_context ioc;
        {
            buffered_read_stream<test::stream, multi_buffer> srs(ioc);
            buffered_read_stream<test::stream, multi_buffer> srs2(std::move(srs));
            srs = std::move(srs2);
#if defined(BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT)
            BEAST_EXPECT(&srs.get_executor().context() == &ioc);
            BEAST_EXPECT(
                &srs.get_executor().context() ==
                &srs2.get_executor().context());
#else
            BEAST_EXPECT(&net::query(srs.get_executor(), net::execution::context) == &ioc);
            BEAST_EXPECT(
                &net::query(srs.get_executor(), net::execution::context) ==
                &net::query(srs2.get_executor(), net::execution::context));
#endif
        }
        {
            test::stream ts{ioc};
            buffered_read_stream<test::stream&, multi_buffer> srs(ts);
        }
    }

    struct loop : std::enable_shared_from_this<loop>
    {
        static std::size_t constexpr limit = 100;
        std::string s_;
        std::size_t n_ = 0;
        std::size_t cap_;
        unit_test::suite& suite_;
        net::io_context& ioc_;
        boost::optional<test::stream> ts_;
        boost::optional<test::fail_count> fc_;
        boost::optional<buffered_read_stream<
            test::stream&, multi_buffer>> brs_;

        loop(
            unit_test::suite& suite,
            net::io_context& ioc,
            std::size_t cap)
            : cap_(cap)
            , suite_(suite)
            , ioc_(ioc)
        {
        }

        void
        run()
        {
            do_read();
        }

        void
        on_read(error_code ec, std::size_t)
        {
            if(! ec)
            {
                suite_.expect(s_ ==
                    "Hello, world!", __FILE__, __LINE__);
                return;
            }
            ++n_;
            if(! suite_.expect(n_ < limit, __FILE__, __LINE__))
                return;
            s_.clear();
            do_read();
        }

        void
        do_read()
        {
            s_.resize(13);
            fc_.emplace(n_);
            ts_.emplace(ioc_, *fc_, ", world!");
            brs_.emplace(*ts_);
            brs_->buffer().commit(net::buffer_copy(
                brs_->buffer().prepare(5), net::buffer("Hello", 5)));
            net::async_read(*brs_,
                net::buffer(&s_[0], s_.size()),
                    bind_front_handler(
                        &loop::on_read,
                        shared_from_this()));
        }
    };

    void
    testAsyncLoop()
    {
        std::make_shared<loop>(*this, ioc_, 0)->run();
        std::make_shared<loop>(*this, ioc_, 3)->run();
    }

    void testRead(yield_context do_yield)
    {
        static std::size_t constexpr limit = 100;
        std::size_t n;
        std::string s;
        s.resize(13);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts(ioc_, fc, ", world!");
            buffered_read_stream<
                test::stream&, multi_buffer> srs(ts);
            srs.buffer().commit(net::buffer_copy(
                srs.buffer().prepare(5), net::buffer("Hello", 5)));
            error_code ec = test::error::test_failure;
            net::read(srs, net::buffer(&s[0], s.size()), ec);
            if(! ec)
            {
                BEAST_EXPECT(s == "Hello, world!");
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts(ioc_, fc, ", world!");
            buffered_read_stream<
                test::stream&, multi_buffer> srs(ts);
            srs.capacity(3);
            srs.buffer().commit(net::buffer_copy(
                srs.buffer().prepare(5), net::buffer("Hello", 5)));
            error_code ec = test::error::test_failure;
            net::read(srs, net::buffer(&s[0], s.size()), ec);
            if(! ec)
            {
                BEAST_EXPECT(s == "Hello, world!");
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts(ioc_, fc, ", world!");
            buffered_read_stream<
                test::stream&, multi_buffer> srs(ts);
            srs.buffer().commit(net::buffer_copy(
                srs.buffer().prepare(5), net::buffer("Hello", 5)));
            error_code ec = test::error::test_failure;
            net::async_read(
                srs, net::buffer(&s[0], s.size()), do_yield[ec]);
            if(! ec)
            {
                BEAST_EXPECT(s == "Hello, world!");
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts(ioc_, fc, ", world!");
            buffered_read_stream<
                test::stream&, multi_buffer> srs(ts);
            srs.capacity(3);
            srs.buffer().commit(net::buffer_copy(
                srs.buffer().prepare(5), net::buffer("Hello", 5)));
            error_code ec = test::error::test_failure;
            net::async_read(
                srs, net::buffer(&s[0], s.size()), do_yield[ec]);
            if(! ec)
            {
                BEAST_EXPECT(s == "Hello, world!");
                break;
            }
        }
        BEAST_EXPECT(n < limit);
    }

    struct copyable_handler
    {
        template<class... Args>
        void
        operator()(Args&&...) const
        {
        }
    };

#if BOOST_ASIO_HAS_CO_AWAIT
    void testAwaitableCompiles(
        buffered_read_stream<test::stream, flat_buffer>& stream,
        net::mutable_buffer rxbuf,
        net::const_buffer txbuf)
    {
        static_assert(std::is_same_v<
            net::awaitable<std::size_t>, decltype(
            stream.async_read_some(rxbuf, net::use_awaitable))>);

        static_assert(std::is_same_v<
            net::awaitable<std::size_t>, decltype(
            stream.async_write_some(txbuf, net::use_awaitable))>);
    }
#endif

    void run() override
    {
        testSpecialMembers();

        yield_to([&](yield_context yield)
        {
            testRead(yield);
        });
        testAsyncLoop();

#if BOOST_ASIO_HAS_CO_AWAIT
        boost::ignore_unused(&buffered_read_stream_test::testAwaitableCompiles);
#endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffered_read_stream);

} // beast
} // boost
