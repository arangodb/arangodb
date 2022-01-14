//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_WEBSOCKET_HPP
#define BOOST_BEAST_TEST_WEBSOCKET_HPP

#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <cstdlib>
#include <memory>
#include <ostream>
#include <thread>

namespace boost {
namespace beast {
namespace test {

// DEPRECATED
class ws_echo_server
{
    std::ostream& log_;
    net::io_context ioc_;
    net::executor_work_guard<
        net::io_context::executor_type> work_;
    multi_buffer buffer_;
    test::stream ts_;
    std::thread t_;
    websocket::stream<test::stream&> ws_;
    bool close_ = false;

public:
    enum class kind
    {
        sync,
        async,
        async_client
    };

    explicit
    ws_echo_server(
        std::ostream& log,
        kind k = kind::sync)
        : log_(log)
        , work_(ioc_.get_executor())
        , ts_(ioc_)
        , ws_(ts_)
    {
        beast::websocket::permessage_deflate pmd;
        pmd.server_enable = true;
        pmd.server_max_window_bits = 9;
        pmd.compLevel = 1;
        ws_.set_option(pmd);

        switch(k)
        {
        case kind::sync:
            t_ = std::thread{[&]{ do_sync(); }};
            break;

        case kind::async:
            t_ = std::thread{[&]{ ioc_.run(); }};
            do_accept();
            break;

        case kind::async_client:
            t_ = std::thread{[&]{ ioc_.run(); }};
            break;
        }
    }

    ~ws_echo_server()
    {
        work_.reset();
        t_.join();
    }

    test::stream&
    stream()
    {
        return ts_;
    }

    void
    async_handshake()
    {
        ws_.async_handshake("localhost", "/",
            std::bind(
                &ws_echo_server::on_handshake,
                this,
                std::placeholders::_1));
    }

    void
    async_close()
    {
        net::post(ioc_,
        [&]
        {
            if(ws_.is_open())
            {
                ws_.async_close({},
                    std::bind(
                        &ws_echo_server::on_close,
                        this,
                        std::placeholders::_1));
            }
            else
            {
                close_ = true;
            }
        });
    }

private:
    void
    do_sync()
    {
        try
        {
            ws_.accept();
            for(;;)
            {
                ws_.read(buffer_);
                ws_.text(ws_.got_text());
                ws_.write(buffer_.data());
                buffer_.consume(buffer_.size());
            }
        }
        catch(system_error const& se)
        {
            boost::ignore_unused(se);
#if 0
            if( se.code() != error::closed &&
                se.code() != error::failed &&
                se.code() != net::error::eof)
                log_ << "ws_echo_server: " << se.code().message() << std::endl;
#endif
        }
        catch(std::exception const& e)
        {
            log_ << "ws_echo_server: " << e.what() << std::endl;
        }
    }

    void
    do_accept()
    {
        ws_.async_accept(std::bind(
            &ws_echo_server::on_accept,
            this,
            std::placeholders::_1));
    }

    void
    on_handshake(error_code ec)
    {
        if(ec)
            return fail(ec);

        do_read();
    }

    void
    on_accept(error_code ec)
    {
        if(ec)
            return fail(ec);

        if(close_)
        {
            return ws_.async_close({},
                std::bind(
                    &ws_echo_server::on_close,
                    this,
                    std::placeholders::_1));
        }

        do_read();
    }

    void
    do_read()
    {
        ws_.async_read(buffer_,
            std::bind(
                &ws_echo_server::on_read,
                this,
                std::placeholders::_1));
    }

    void
    on_read(error_code ec)
    {
        if(ec)
            return fail(ec);
        ws_.text(ws_.got_text());
        ws_.async_write(buffer_.data(),
            std::bind(
                &ws_echo_server::on_write,
                this,
                std::placeholders::_1));
    }

    void
    on_write(error_code ec)
    {
        if(ec)
            return fail(ec);
        buffer_.consume(buffer_.size());
        do_read();
    }

    void
    on_close(error_code ec)
    {
        if(ec)
            return fail(ec);
    }

    void
    fail(error_code ec)
    {
        boost::ignore_unused(ec);
#if 0
        if( ec != error::closed &&
            ec != error::failed &&
            ec != net::error::eof)
            log_ <<
                "echo_server_async: " <<
                ec.message() <<
                std::endl;
#endif
    }
};

} // test
} // beast
} // boost

#endif
