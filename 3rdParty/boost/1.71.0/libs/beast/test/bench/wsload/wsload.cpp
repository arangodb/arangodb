//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// wsload
//
//  Measure the performance of a WebSocket server
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/_experimental/unit_test/dstream.hpp>
#include <boost/asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class test_buffer
{
    char data_[4096];
    net::const_buffer b_;

public:
    using const_iterator =
        net::const_buffer const*;

    using value_type = net::const_buffer;

    test_buffer()
        : b_(data_, sizeof(data_))
    {
        std::mt19937_64 rng;
        std::uniform_int_distribution<unsigned short> dist;
        for(auto& c : data_)
            c = static_cast<unsigned char>(dist(rng));
    }

    const_iterator
    begin() const
    {
        return &b_;
    }

    const_iterator
    end() const
    {
        return begin() + 1;
    }
};

class report
{
    std::mutex m_;
    std::size_t bytes_ = 0;
    std::size_t messages_ = 0;

public:
    void
    insert(std::size_t messages, std::size_t bytes)
    {
        std::lock_guard<std::mutex> lock(m_);
        bytes_ += bytes;
        messages_ += messages;
    }

    std::size_t
    bytes() const
    {
        return bytes_;
    }

    std::size_t
    messages() const
    {
        return messages_;
    }
};

void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

class connection
    : public std::enable_shared_from_this<connection>
{
    websocket::stream<tcp::socket> ws_;
    tcp::endpoint ep_;
    std::size_t messages_;
    report& rep_;
    test_buffer const& tb_;
    net::strand<
        net::io_context::executor_type> strand_;
    beast::flat_buffer buffer_;
    std::mt19937_64 rng_;
    std::size_t count_ = 0;
    std::size_t bytes_ = 0;

public:
    connection(
        net::io_context& ioc,
        tcp::endpoint const& ep,
        std::size_t messages,
        bool deflate,
        report& rep,
        test_buffer const& tb)
        : ws_(ioc)
        , ep_(ep)
        , messages_(messages)
        , rep_(rep)
        , tb_(tb)
        , strand_(ioc.get_executor())
    {
        websocket::permessage_deflate pmd;
        pmd.client_enable = deflate;
        ws_.set_option(pmd);
        ws_.binary(true);
        ws_.auto_fragment(false);
        ws_.write_buffer_bytes(64 * 1024);
    }

    ~connection()
    {
        rep_.insert(count_, bytes_);
    }

    void
    run()
    {
        ws_.next_layer().async_connect(ep_,
            beast::bind_front_handler(
                &connection::on_connect,
                this->shared_from_this()));
    }

private:
    void
    on_connect(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "on_connect");

        ws_.async_handshake(
            ep_.address().to_string() + ":" + std::to_string(ep_.port()),
            "/",
            beast::bind_front_handler(
                &connection::on_handshake,
                this->shared_from_this()));
    }

    void
    on_handshake(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "handshake");

        do_write();
    }

    void
    do_write()
    {
        std::geometric_distribution<std::size_t> dist{
            double(4) / beast::buffer_bytes(tb_)};
        ws_.async_write_some(true,
            beast::buffers_prefix(dist(rng_), tb_),
            beast::bind_front_handler(
                &connection::on_write,
                this->shared_from_this()));
    }

    void
    on_write(beast::error_code ec, std::size_t)
    {
        if(ec)
            return fail(ec, "write");

        if(messages_--)
            return do_read();

        ws_.async_close({},
            beast::bind_front_handler(
                &connection::on_close,
                this->shared_from_this()));
    }

    void
    do_read()
    {
        ws_.async_read(buffer_,
            beast::bind_front_handler(
                &connection::on_read,
                this->shared_from_this()));
    }

    void
    on_read(beast::error_code ec, std::size_t)
    {
        if(ec)
            return fail(ec, "read");

        ++count_;
        bytes_ += buffer_.size();
        buffer_.consume(buffer_.size());
        do_write();
    }

    void
    on_close(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "close");
    }
};

class timer
{
    using clock_type =
        std::chrono::system_clock;

    clock_type::time_point when_;

public:
    using duration =
        clock_type::duration;

    timer()
        : when_(clock_type::now())
    {
    }

    duration
    elapsed() const
    {
        return clock_type::now() - when_;
    }
};

inline
std::uint64_t
throughput(
    std::chrono::duration<double> const& elapsed,
    std::uint64_t items)
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        1 / (elapsed/items).count());
}

int
main(int argc, char** argv)
{
    beast::unit_test::dstream dout(std::cerr);

    try
    {
        // Check command line arguments.
        if(argc != 8)
        {
            std::cerr <<
                "Usage: bench-wsload <address> <port> <trials> <messages> <workers> <threads> <compression:0|1>";
            return EXIT_FAILURE;
        }

        auto const address = net::ip::make_address(argv[1]);
        auto const port    = static_cast<unsigned short>(std::atoi(argv[2]));
        auto const trials  = static_cast<std::size_t>(std::atoi(argv[3]));
        auto const messages= static_cast<std::size_t>(std::atoi(argv[4]));
        auto const workers = static_cast<std::size_t>(std::atoi(argv[5]));
        auto const threads = static_cast<std::size_t>(std::atoi(argv[6]));
        auto const deflate = std::atoi(argv[7]) != 0;
        auto const work = (messages + workers - 1) / workers;
        test_buffer tb;
        for(auto i = trials; i != 0; --i)
        {
            report rep;
            net::io_context ioc{1};
            for(auto j = workers; j; --j)
            {
                auto sp =
                std::make_shared<connection>(
                    ioc,
                    tcp::endpoint{address, port},
                    work,
                    deflate,
                    rep,
                    tb);
                sp->run();
            }
            timer clock;
            std::vector<std::thread> tv;
            if(threads > 1)
            {
                tv.reserve(threads);
                tv.emplace_back([&ioc]{ ioc.run(); });
            }
            ioc.run();
            for(auto& t : tv)
                t.join();
            auto const elapsed = clock.elapsed();
            dout <<
                throughput(elapsed, rep.bytes()) << " bytes/s in " <<
                (std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    elapsed).count() / 1000.) << "ms and " <<
                rep.bytes() << " bytes" << std::endl;
        }
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
