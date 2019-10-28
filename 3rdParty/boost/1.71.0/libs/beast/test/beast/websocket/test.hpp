//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BEAST_TEST_WEBSOCKET_TEST_HPP
#define BEAST_TEST_WEBSOCKET_TEST_HPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/optional.hpp>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>

namespace boost {
namespace beast {
namespace websocket {

class websocket_test_suite
    : public beast::unit_test::suite
    , public test::enable_yield_to
{
public:
    template<bool deflateSupported>
    using ws_type_t =
        websocket::stream<test::stream&, deflateSupported>;

    using ws_type =
        websocket::stream<test::stream&>;

    struct move_only_handler
    {
        move_only_handler() = default;
        move_only_handler(move_only_handler&&) = default;
        move_only_handler(move_only_handler const&) = delete;

        template<class... Args>
        void
        operator()(Args&&...) const
        {
        }
    };

    enum class kind
    {
        sync,
        async,
        async_client
    };

    class echo_server
    {
        enum
        {
            buf_size = 20000
        };

        std::ostream& log_;
        net::io_context ioc_;
        net::executor_work_guard<
            net::io_context::executor_type> work_;
        static_buffer<buf_size> buffer_;
        test::stream ts_;
        std::thread t_;
        websocket::stream<test::stream&> ws_;
        bool close_ = false;

    public:
        explicit
        echo_server(
            std::ostream& log,
            kind k = kind::sync)
            : log_(log)
            , work_(ioc_.get_executor())
            , ts_(ioc_)
            , ws_(ts_)
        {
            permessage_deflate pmd;
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

        ~echo_server()
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
                bind_front_handler(
                    &echo_server::on_handshake,
                    this));
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
                        bind_front_handler(
                            &echo_server::on_close,
                            this));
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
                    log_ << "echo_server: " << se.code().message() << std::endl;
    #endif
            }
            catch(std::exception const& e)
            {
                log_ << "echo_server: " << e.what() << std::endl;
            }
        }

        void
        do_accept()
        {
            ws_.async_accept(
                bind_front_handler(
                    &echo_server::on_accept,
                    this));
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
                    bind_front_handler(
                        &echo_server::on_close,
                        this));
            }

            do_read();
        }

        void
        do_read()
        {
            ws_.async_read(buffer_,
                beast::bind_front_handler(
                    &echo_server::on_read,
                    this));
        }

        void
        on_read(error_code ec, std::size_t)
        {
            if(ec)
                return fail(ec);
            ws_.text(ws_.got_text());
            ws_.async_write(buffer_.data(),
                beast::bind_front_handler(
                    &echo_server::on_write,
                    this));
        }

        void
        on_write(error_code ec, std::size_t)
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

    template<class Test>
    void
    doFailLoop(
        Test const& f, std::size_t limit = 200)
    {
        std::size_t n;
        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            try
            {
                f(fc);
                break;
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == test::error::test_failure,
                    se.code().message());
            }
        }
        BEAST_EXPECT(n < limit);
    }

    template<class Test>
    void
    doStreamLoop(Test const& f)
    {
        // This number has to be high for the
        // test that writes the large buffer.
        static std::size_t constexpr limit = 200;

        doFailLoop(
            [&](test::fail_count& fc)
            {
                test::stream ts{ioc_, fc};
                f(ts);
                ts.close();
            }
            , limit);
    }

    template<bool deflateSupported = true, class Test>
    void
    doTest(
        permessage_deflate const& pmd,
        Test const& f)
    {
        // This number has to be high for the
        // test that writes the large buffer.
        static std::size_t constexpr limit = 200;

        for(int i = 0; i < 2; ++i)
        {
            std::size_t n;
            for(n = 0; n < limit; ++n)
            {
                test::fail_count fc{n};
                test::stream ts{ioc_, fc};
                ws_type_t<deflateSupported> ws{ts};
                ws.set_option(pmd);

                echo_server es{log, i==1 ?
                    kind::async : kind::sync};
                error_code ec;
                ws.next_layer().connect(es.stream());
                ws.handshake("localhost", "/", ec);
                if(ec)
                {
                    ts.close();
                    if( ! BEAST_EXPECTS(
                            ec == test::error::test_failure,
                            ec.message()))
                        BOOST_THROW_EXCEPTION(system_error{ec});
                    continue;
                }
                try
                {
                    f(ws);
                    ts.close();
                    break;
                }
                catch(system_error const& se)
                {
                    BEAST_EXPECTS(
                        se.code() == test::error::test_failure,
                        se.code().message());
                }
                catch(std::exception const& e)
                {
                    fail(e.what(), __FILE__, __LINE__);
                }
                ts.close();
                continue;
            }
            BEAST_EXPECT(n < limit);
        }
    }

    //--------------------------------------------------------------------------

    net::const_buffer cbuf(std::initializer_list<std::uint8_t> bytes)
    {
        return {bytes.begin(), bytes.size()};
    }

    template<std::size_t N>
    static
    net::const_buffer
    sbuf(const char (&s)[N])
    {
        return net::const_buffer(&s[0], N-1);
    }

    template<
        class DynamicBuffer,
        class ConstBufferSequence>
    void
    put(
        DynamicBuffer& buffer,
        ConstBufferSequence const& buffers)
    {
        buffer.commit(net::buffer_copy(
            buffer.prepare(buffer_bytes(buffers)),
            buffers));
    }

    template<class Pred>
    bool
    run_until(net::io_context& ioc,
        std::size_t limit, Pred&& pred)
    {
        for(std::size_t i = 0; i < limit; ++i)
        {
            if(pred())
                return true;
            ioc.run_one();
        }
        return false;
    }

    template<class Pred>
    bool
    run_until(
        net::io_context& ioc, Pred&& pred)
    {
        return run_until(ioc, 100, pred);
    }

    inline
    std::string const&
    random_string()
    {
        static std::string const s = []
        {
            std::size_t constexpr N = 16384;
            std::mt19937 mt{1};
            std::string tmp;
            tmp.reserve(N);
            for(std::size_t i = 0; i < N; ++ i)
                tmp.push_back(static_cast<char>(
                    std::uniform_int_distribution<
                        unsigned>{0, 255}(mt)));
            return tmp;
        }();
        return s;
    }

    //--------------------------------------------------------------------------

    struct SyncClient
    {
        template<class NextLayer, bool deflateSupported>
        void
        accept(
            stream<NextLayer, deflateSupported>& ws) const
        {
            ws.accept();
        }

        template<
            class NextLayer, bool deflateSupported,
            class Buffers>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept(stream<NextLayer, deflateSupported>& ws,
            Buffers const& buffers) const
        {
            ws.accept(buffers);
        }

        template<class NextLayer, bool deflateSupported>
        void
        accept(
            stream<NextLayer, deflateSupported>& ws,
            http::request<http::empty_body> const& req) const
        {
            ws.accept(req);
        }

        template<
            class NextLayer, bool deflateSupported,
            class Decorator>
        void
        accept_ex(
            stream<NextLayer, deflateSupported>& ws,
            Decorator const& d) const
        {
            ws.accept_ex(d);
        }

        template<
            class NextLayer, bool deflateSupported,
            class Buffers, class Decorator>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept_ex(
            stream<NextLayer, deflateSupported>& ws,
            Buffers const& buffers,
            Decorator const& d) const
        {
            ws.accept_ex(buffers, d);
        }

        template<
            class NextLayer, bool deflateSupported,
            class Decorator>
        void
        accept_ex(
            stream<NextLayer, deflateSupported>& ws,
            http::request<http::empty_body> const& req,
            Decorator const& d) const
        {
            ws.accept_ex(req, d);
        }

        template<
            class NextLayer, bool deflateSupported,
            class Buffers, class Decorator>
        void
        accept_ex(
            stream<NextLayer, deflateSupported>& ws,
            http::request<http::empty_body> const& req,
            Buffers const& buffers,
            Decorator const& d) const
        {
            ws.accept_ex(req, buffers, d);
        }

        template<class NextLayer, bool deflateSupported>
        void
        handshake(
            stream<NextLayer, deflateSupported>& ws,
            string_view uri,
            string_view path) const
        {
            ws.handshake(uri, path);
        }

        template<class NextLayer, bool deflateSupported>
        void
        handshake(
            stream<NextLayer, deflateSupported>& ws,
            response_type& res,
            string_view uri,
            string_view path) const
        {
            ws.handshake(res, uri, path);
        }

        template<
            class NextLayer, bool deflateSupported,
            class Decorator>
        void
        handshake_ex(
            stream<NextLayer, deflateSupported>& ws,
            string_view uri,
            string_view path,
            Decorator const& d) const
        {
            ws.handshake_ex(uri, path, d);
        }

        template<
            class NextLayer, bool deflateSupported,
            class Decorator>
        void
        handshake_ex(
            stream<NextLayer, deflateSupported>& ws,
            response_type& res,
            string_view uri,
            string_view path,
            Decorator const& d) const
        {
            ws.handshake_ex(res, uri, path, d);
        }

        template<class NextLayer, bool deflateSupported>
        void
        ping(stream<NextLayer, deflateSupported>& ws,
            ping_data const& payload) const
        {
            ws.ping(payload);
        }

        template<class NextLayer, bool deflateSupported>
        void
        pong(stream<NextLayer, deflateSupported>& ws,
            ping_data const& payload) const
        {
            ws.pong(payload);
        }

        template<class NextLayer, bool deflateSupported>
        void
        close(stream<NextLayer, deflateSupported>& ws,
            close_reason const& cr) const
        {
            ws.close(cr);
        }

        template<
            class NextLayer, bool deflateSupported,
            class DynamicBuffer>
        std::size_t
        read(stream<NextLayer, deflateSupported>& ws,
            DynamicBuffer& buffer) const
        {
            return ws.read(buffer);
        }

        template<
            class NextLayer, bool deflateSupported,
            class DynamicBuffer>
        std::size_t
        read_some(
            stream<NextLayer, deflateSupported>& ws,
            std::size_t limit,
            DynamicBuffer& buffer) const
        {
            return ws.read_some(buffer, limit);
        }

        template<
            class NextLayer, bool deflateSupported,
            class MutableBufferSequence>
        std::size_t
        read_some(
            stream<NextLayer, deflateSupported>& ws,
            MutableBufferSequence const& buffers) const
        {
            return ws.read_some(buffers);
        }

        template<
            class NextLayer, bool deflateSupported,
            class ConstBufferSequence>
        std::size_t
        write(
            stream<NextLayer, deflateSupported>& ws,
            ConstBufferSequence const& buffers) const
        {
            return ws.write(buffers);
        }

        template<
            class NextLayer, bool deflateSupported,
            class ConstBufferSequence>
        std::size_t
        write_some(
            stream<NextLayer, deflateSupported>& ws,
            bool fin,
            ConstBufferSequence const& buffers) const
        {
            return ws.write_some(fin, buffers);
        }

        template<
            class NextLayer, bool deflateSupported,
            class ConstBufferSequence>
        std::size_t
        write_raw(
            stream<NextLayer, deflateSupported>& ws,
            ConstBufferSequence const& buffers) const
        {
            return net::write(
                ws.next_layer(), buffers);
        }
    };

    //--------------------------------------------------------------------------

    class AsyncClient
    {
        net::yield_context& yield_;

    public:
        explicit
        AsyncClient(net::yield_context& yield)
            : yield_(yield)
        {
        }

        template<class NextLayer, bool deflateSupported>
        void
        accept(stream<NextLayer, deflateSupported>& ws) const
        {
            error_code ec;
            ws.async_accept(yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported,
            class Buffers>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept(
            stream<NextLayer, deflateSupported>& ws,
            Buffers const& buffers) const
        {
            error_code ec;
            ws.async_accept(buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, bool deflateSupported>
        void
        accept(
            stream<NextLayer, deflateSupported>& ws,
            http::request<http::empty_body> const& req) const
        {
            error_code ec;
            ws.async_accept(req, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported,
            class Decorator>
        void
        accept_ex(
            stream<NextLayer, deflateSupported>& ws,
            Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported,
            class Buffers, class Decorator>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept_ex(
            stream<NextLayer, deflateSupported>& ws,
            Buffers const& buffers,
            Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(buffers, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported,
            class Decorator>
        void
        accept_ex(
            stream<NextLayer, deflateSupported>& ws,
            http::request<http::empty_body> const& req,
            Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(req, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported,
            class Buffers, class Decorator>
        void
        accept_ex(
            stream<NextLayer, deflateSupported>& ws,
            http::request<http::empty_body> const& req,
            Buffers const& buffers,
            Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(
                req, buffers, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported>
        void
        handshake(
            stream<NextLayer, deflateSupported>& ws,
            string_view uri,
            string_view path) const
        {
            error_code ec;
            ws.async_handshake(
                uri, path, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, bool deflateSupported>
        void
        handshake(
            stream<NextLayer, deflateSupported>& ws,
            response_type& res,
            string_view uri,
            string_view path) const
        {
            error_code ec;
            ws.async_handshake(
                res, uri, path, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported,
            class Decorator>
        void
        handshake_ex(
            stream<NextLayer, deflateSupported>& ws,
            string_view uri,
            string_view path,
            Decorator const &d) const
        {
            error_code ec;
            ws.async_handshake_ex(
                uri, path, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported,
            class Decorator>
        void
        handshake_ex(
            stream<NextLayer, deflateSupported>& ws,
            response_type& res,
            string_view uri,
            string_view path,
            Decorator const &d) const
        {
            error_code ec;
            ws.async_handshake_ex(
                res, uri, path, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, bool deflateSupported>
        void
        ping(
            stream<NextLayer, deflateSupported>& ws,
            ping_data const& payload) const
        {
            error_code ec;
            ws.async_ping(payload, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, bool deflateSupported>
        void
        pong(
            stream<NextLayer, deflateSupported>& ws,
            ping_data const& payload) const
        {
            error_code ec;
            ws.async_pong(payload, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, bool deflateSupported>
        void
        close(
            stream<NextLayer, deflateSupported>& ws,
            close_reason const& cr) const
        {
            error_code ec;
            ws.async_close(cr, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, bool deflateSupported,
            class DynamicBuffer>
        std::size_t
        read(
            stream<NextLayer, deflateSupported>& ws,
            DynamicBuffer& buffer) const
        {
            error_code ec;
            auto const bytes_written =
                ws.async_read(buffer, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_written;
        }

        template<
            class NextLayer, bool deflateSupported,
            class DynamicBuffer>
        std::size_t
        read_some(
            stream<NextLayer, deflateSupported>& ws,
            std::size_t limit,
            DynamicBuffer& buffer) const
        {
            error_code ec;
            auto const bytes_written =
                ws.async_read_some(buffer, limit, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_written;
        }

        template<
            class NextLayer, bool deflateSupported,
            class MutableBufferSequence>
        std::size_t
        read_some(
            stream<NextLayer, deflateSupported>& ws,
            MutableBufferSequence const& buffers) const
        {
            error_code ec;
            auto const bytes_written =
                ws.async_read_some(buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_written;
        }

        template<
            class NextLayer, bool deflateSupported,
            class ConstBufferSequence>
        std::size_t
        write(
            stream<NextLayer, deflateSupported>& ws,
            ConstBufferSequence const& buffers) const
        {
            error_code ec;
            auto const bytes_transferred =
                ws.async_write(buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_transferred;
        }

        template<
            class NextLayer, bool deflateSupported,
            class ConstBufferSequence>
        std::size_t
        write_some(
            stream<NextLayer, deflateSupported>& ws,
            bool fin,
            ConstBufferSequence const& buffers) const
        {
            error_code ec;
            auto const bytes_transferred =
                ws.async_write_some(fin, buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_transferred;
        }

        template<
            class NextLayer, bool deflateSupported,
            class ConstBufferSequence>
        std::size_t
        write_raw(
            stream<NextLayer, deflateSupported>& ws,
            ConstBufferSequence const& buffers) const
        {
            error_code ec;
            auto const bytes_transferred =
                net::async_write(
                    ws.next_layer(), buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_transferred;
        }
    };
};

struct test_sync_api
{
    template<class NextLayer, bool deflateSupported>
    void
    accept(
        stream<NextLayer, deflateSupported>& ws) const
    {
        ws.accept();
    }

    template<
        class NextLayer, bool deflateSupported,
        class Buffers>
    typename std::enable_if<
        ! http::detail::is_header<Buffers>::value>::type
    accept(stream<NextLayer, deflateSupported>& ws,
        Buffers const& buffers) const
    {
        ws.accept(buffers);
    }

    template<class NextLayer, bool deflateSupported>
    void
    accept(
        stream<NextLayer, deflateSupported>& ws,
        http::request<http::empty_body> const& req) const
    {
        ws.accept(req);
    }

    template<
        class NextLayer, bool deflateSupported,
        class Decorator>
    void
    accept_ex(
        stream<NextLayer, deflateSupported>& ws,
        Decorator const& d) const
    {
        ws.accept_ex(d);
    }

    template<
        class NextLayer, bool deflateSupported,
        class Buffers, class Decorator>
    typename std::enable_if<
        ! http::detail::is_header<Buffers>::value>::type
    accept_ex(
        stream<NextLayer, deflateSupported>& ws,
        Buffers const& buffers,
        Decorator const& d) const
    {
        ws.accept_ex(buffers, d);
    }

    template<
        class NextLayer, bool deflateSupported,
        class Decorator>
    void
    accept_ex(
        stream<NextLayer, deflateSupported>& ws,
        http::request<http::empty_body> const& req,
        Decorator const& d) const
    {
        ws.accept_ex(req, d);
    }

    template<
        class NextLayer, bool deflateSupported,
        class Buffers, class Decorator>
    void
    accept_ex(
        stream<NextLayer, deflateSupported>& ws,
        http::request<http::empty_body> const& req,
        Buffers const& buffers,
        Decorator const& d) const
    {
        ws.accept_ex(req, buffers, d);
    }

    template<class NextLayer, bool deflateSupported>
    void
    handshake(
        stream<NextLayer, deflateSupported>& ws,
        response_type& res,
        string_view uri,
        string_view path) const
    {
        ws.handshake(res, uri, path);
    }

    template<
        class NextLayer, bool deflateSupported,
        class Decorator>
    void
    handshake_ex(
        stream<NextLayer, deflateSupported>& ws,
        string_view uri,
        string_view path,
        Decorator const& d) const
    {
        ws.handshake_ex(uri, path, d);
    }

    template<
        class NextLayer, bool deflateSupported,
        class Decorator>
    void
    handshake_ex(
        stream<NextLayer, deflateSupported>& ws,
        response_type& res,
        string_view uri,
        string_view path,
        Decorator const& d) const
    {
        ws.handshake_ex(res, uri, path, d);
    }

    template<class NextLayer, bool deflateSupported>
    void
    ping(stream<NextLayer, deflateSupported>& ws,
        ping_data const& payload) const
    {
        ws.ping(payload);
    }

    template<class NextLayer, bool deflateSupported>
    void
    pong(stream<NextLayer, deflateSupported>& ws,
        ping_data const& payload) const
    {
        ws.pong(payload);
    }

    template<class NextLayer, bool deflateSupported>
    void
    close(stream<NextLayer, deflateSupported>& ws,
        close_reason const& cr) const
    {
        ws.close(cr);
    }

    template<
        class NextLayer, bool deflateSupported,
        class DynamicBuffer>
    std::size_t
    read(stream<NextLayer, deflateSupported>& ws,
        DynamicBuffer& buffer) const
    {
        return ws.read(buffer);
    }

    template<
        class NextLayer, bool deflateSupported,
        class DynamicBuffer>
    std::size_t
    read_some(
        stream<NextLayer, deflateSupported>& ws,
        std::size_t limit,
        DynamicBuffer& buffer) const
    {
        return ws.read_some(buffer, limit);
    }

    template<
        class NextLayer, bool deflateSupported,
        class MutableBufferSequence>
    std::size_t
    read_some(
        stream<NextLayer, deflateSupported>& ws,
        MutableBufferSequence const& buffers) const
    {
        return ws.read_some(buffers);
    }

    template<
        class NextLayer, bool deflateSupported,
        class ConstBufferSequence>
    std::size_t
    write(
        stream<NextLayer, deflateSupported>& ws,
        ConstBufferSequence const& buffers) const
    {
        return ws.write(buffers);
    }

    template<
        class NextLayer, bool deflateSupported,
        class ConstBufferSequence>
    std::size_t
    write_some(
        stream<NextLayer, deflateSupported>& ws,
        bool fin,
        ConstBufferSequence const& buffers) const
    {
        return ws.write_some(fin, buffers);
    }

    template<
        class NextLayer, bool deflateSupported,
        class ConstBufferSequence>
    std::size_t
    write_raw(
        stream<NextLayer, deflateSupported>& ws,
        ConstBufferSequence const& buffers) const
    {
        return net::write(
            ws.next_layer(), buffers);
    }
};

//--------------------------------------------------------------------------

class test_async_api
{
    struct handler
    {
        error_code& ec_;
        std::size_t* n_ = 0;
        bool pass_ = false;

        explicit
        handler(error_code& ec)
            : ec_(ec)
        {
        }

        explicit
        handler(error_code& ec, std::size_t& n)
            : ec_(ec)
            , n_(&n)
        {
            *n_ = 0;
        }

        handler(handler&& other)
            : ec_(other.ec_)
            , pass_(boost::exchange(other.pass_, true))
        {
        }

        ~handler()
        {
            BEAST_EXPECT(pass_);
        }

        void
        operator()(error_code ec)
        {
            BEAST_EXPECT(! pass_);
            pass_ = true;
            ec_ = ec;
        }

        void
        operator()(error_code ec, std::size_t n)
        {
            BEAST_EXPECT(! pass_);
            pass_ = true;
            ec_ = ec;
            if(n_)
                *n_ = n;
        }
    };

public:
    template<class NextLayer, bool deflateSupported>
    void
    accept(
        stream<NextLayer, deflateSupported>& ws) const
    {
        error_code ec;
        ws.async_accept(handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported,
        class Buffers>
    typename std::enable_if<
        ! http::detail::is_header<Buffers>::value>::type
    accept(
        stream<NextLayer, deflateSupported>& ws,
        Buffers const& buffers) const
    {
        error_code ec;
        ws.async_accept(buffers, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<class NextLayer, bool deflateSupported>
    void
    accept(
        stream<NextLayer, deflateSupported>& ws,
        http::request<http::empty_body> const& req) const
    {
        error_code ec;
        ws.async_accept(req, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported,
        class Decorator>
    void
    accept_ex(
        stream<NextLayer, deflateSupported>& ws,
        Decorator const& d) const
    {
        error_code ec;
        ws.async_accept_ex(d, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported,
        class Buffers, class Decorator>
    typename std::enable_if<
        ! http::detail::is_header<Buffers>::value>::type
    accept_ex(
        stream<NextLayer, deflateSupported>& ws,
        Buffers const& buffers,
        Decorator const& d) const
    {
        error_code ec;
        ws.async_accept_ex(buffers, d, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported,
        class Decorator>
    void
    accept_ex(
        stream<NextLayer, deflateSupported>& ws,
        http::request<http::empty_body> const& req,
        Decorator const& d) const
    {
        error_code ec;
        ws.async_accept_ex(req, d, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported,
        class Buffers, class Decorator>
    void
    accept_ex(
        stream<NextLayer, deflateSupported>& ws,
        http::request<http::empty_body> const& req,
        Buffers const& buffers,
        Decorator const& d) const
    {
        error_code ec;
        ws.async_accept_ex(
            req, buffers, d, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported>
    void
    handshake(
        stream<NextLayer, deflateSupported>& ws,
        string_view uri,
        string_view path) const
    {
        error_code ec;
        ws.async_handshake(
            uri, path, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<class NextLayer, bool deflateSupported>
    void
    handshake(
        stream<NextLayer, deflateSupported>& ws,
        response_type& res,
        string_view uri,
        string_view path) const
    {
        error_code ec;
        ws.async_handshake(
            res, uri, path, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported,
        class Decorator>
    void
    handshake_ex(
        stream<NextLayer, deflateSupported>& ws,
        string_view uri,
        string_view path,
        Decorator const &d) const
    {
        error_code ec;
        ws.async_handshake_ex(
            uri, path, d, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported,
        class Decorator>
    void
    handshake_ex(
        stream<NextLayer, deflateSupported>& ws,
        response_type& res,
        string_view uri,
        string_view path,
        Decorator const &d) const
    {
        error_code ec;
        ws.async_handshake_ex(
            res, uri, path, d, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<class NextLayer, bool deflateSupported>
    void
    ping(
        stream<NextLayer, deflateSupported>& ws,
        ping_data const& payload) const
    {
        error_code ec;
        ws.async_ping(payload, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<class NextLayer, bool deflateSupported>
    void
    pong(
        stream<NextLayer, deflateSupported>& ws,
        ping_data const& payload) const
    {
        error_code ec;
        ws.async_pong(payload, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<class NextLayer, bool deflateSupported>
    void
    close(
        stream<NextLayer, deflateSupported>& ws,
        close_reason const& cr) const
    {
        error_code ec;
        ws.async_close(cr, handler(ec));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
    }

    template<
        class NextLayer, bool deflateSupported,
        class DynamicBuffer>
    std::size_t
    read(
        stream<NextLayer, deflateSupported>& ws,
        DynamicBuffer& buffer) const
    {
        error_code ec;
        std::size_t n;
        ws.async_read(buffer, handler(ec, n));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
        return n;
    }

    template<
        class NextLayer, bool deflateSupported,
        class DynamicBuffer>
    std::size_t
    read_some(
        stream<NextLayer, deflateSupported>& ws,
        std::size_t limit,
        DynamicBuffer& buffer) const
    {
        error_code ec;
        std::size_t n;
        ws.async_read_some(buffer, limit, handler(ec, n));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
        return n;
    }

    template<
        class NextLayer, bool deflateSupported,
        class MutableBufferSequence>
    std::size_t
    read_some(
        stream<NextLayer, deflateSupported>& ws,
        MutableBufferSequence const& buffers) const
    {
        error_code ec;
        std::size_t n;
        ws.async_read_some(buffers, handler(ec, n));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
        return n;
    }

    template<
        class NextLayer, bool deflateSupported,
        class ConstBufferSequence>
    std::size_t
    write(
        stream<NextLayer, deflateSupported>& ws,
        ConstBufferSequence const& buffers) const
    {
        error_code ec;
        std::size_t n;
        ws.async_write(buffers, handler(ec, n));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
        return n;
    }

    template<
        class NextLayer, bool deflateSupported,
        class ConstBufferSequence>
    std::size_t
    write_some(
        stream<NextLayer, deflateSupported>& ws,
        bool fin,
        ConstBufferSequence const& buffers) const
    {
        error_code ec;
        std::size_t n;
        ws.async_write_some(fin, buffers, handler(ec, n));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
        return n;
    }

    template<
        class NextLayer, bool deflateSupported,
        class ConstBufferSequence>
    std::size_t
    write_raw(
        stream<NextLayer, deflateSupported>& ws,
        ConstBufferSequence const& buffers) const
    {
        error_code ec;
        std::size_t n;
        net::async_write(ws.next_layer(),
            buffers, handler(ec, n));
        ws.get_executor().context().run();
        ws.get_executor().context().restart();
        if(ec)
            throw system_error{ec};
        return n;
    }
};

} // websocket
} // beast
} // boost

#endif
