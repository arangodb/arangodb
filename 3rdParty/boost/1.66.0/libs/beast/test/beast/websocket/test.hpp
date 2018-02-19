//
// Copyright (w) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BEAST_TEST_WEBSOCKET_TEST_HPP
#define BEAST_TEST_WEBSOCKET_TEST_HPP

#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/test/stream.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>
#include <array>
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
    using ws_type =
        websocket::stream<test::stream&>;

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
        boost::asio::io_context ioc_;
        boost::optional<
            boost::asio::executor_work_guard<
                boost::asio::io_context::executor_type>> work_;
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
            work_ = boost::none;
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
                    &echo_server::on_handshake,
                    this,
                    std::placeholders::_1));
        }

        void
        async_close()
        {
            boost::asio::post(ioc_,
            [&]
            {
                if(ws_.is_open())
                {
                    ws_.async_close({},
                        std::bind(
                            &echo_server::on_close,
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
                    se.code() != boost::asio::error::eof)
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
            ws_.async_accept(std::bind(
                &echo_server::on_accept,
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
                        &echo_server::on_close,
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
                    &echo_server::on_read,
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
                    &echo_server::on_write,
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
                ec != boost::asio::error::eof)
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
            test::fail_counter fc{n};
            try
            {
                f(fc);
                break;
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == test::error::fail_error,
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
            [&](test::fail_counter& fc)
            {
                test::stream ts{ioc_, fc};
                f(ts);
                ts.close();
            }
            , limit);
    }

    template<class Test>
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
                test::fail_counter fc{n};
                test::stream ts{ioc_, fc};
                ws_type ws{ts};
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
                            ec == test::error::fail_error,
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
                        se.code() == test::error::fail_error,
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

    template<class ConstBufferSequence>
    std::string
    to_string(ConstBufferSequence const& bs)
    {
        std::string s;
        s.reserve(buffer_size(bs));
        for(auto b : beast::detail::buffers_range(bs))
            s.append(
                reinterpret_cast<char const*>(b.data()),
                b.size());
        return s;
    }

    template<std::size_t N>
    class cbuf_helper
    {
        std::array<std::uint8_t, N> v_;
        boost::asio::const_buffer cb_;

    public:
        using value_type = decltype(cb_);
        using const_iterator = value_type const*;

        template<class... Vn>
        explicit
        cbuf_helper(Vn... vn)
            : v_({{ static_cast<std::uint8_t>(vn)... }})
            , cb_(v_.data(), v_.size())
        {
        }

        const_iterator
        begin() const
        {
            return &cb_;
        }

        const_iterator
        end() const
        {
            return begin()+1;
        }
    };

    template<class... Vn>
    cbuf_helper<sizeof...(Vn)>
    cbuf(Vn... vn)
    {
        return cbuf_helper<sizeof...(Vn)>(vn...);
    }

    template<std::size_t N>
    static
    boost::asio::const_buffer
    sbuf(const char (&s)[N])
    {
        return boost::asio::const_buffer(&s[0], N-1);
    }

    template<
        class DynamicBuffer,
        class ConstBufferSequence>
    void
    put(
        DynamicBuffer& buffer,
        ConstBufferSequence const& buffers)
    {
        using boost::asio::buffer_copy;
        using boost::asio::buffer_size;
        buffer.commit(buffer_copy(
            buffer.prepare(buffer_size(buffers)),
            buffers));
    }

    template<class Pred>
    bool
    run_until(boost::asio::io_context& ioc,
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
        boost::asio::io_context& ioc, Pred&& pred)
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
        template<class NextLayer>
        void
        accept(stream<NextLayer>& ws) const
        {
            ws.accept();
        }

        template<class NextLayer, class Buffers>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept(stream<NextLayer>& ws,
            Buffers const& buffers) const
        {
            ws.accept(buffers);
        }

        template<class NextLayer>
        void
        accept(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req) const
        {
            ws.accept(req);
        }

        template<class NextLayer, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            Decorator const& d) const
        {
            ws.accept_ex(d);
        }

        template<class NextLayer,
            class Buffers, class Decorator>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept_ex(stream<NextLayer>& ws,
            Buffers const& buffers,
                Decorator const& d) const
        {
            ws.accept_ex(buffers, d);
        }

        template<class NextLayer, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req,
                Decorator const& d) const
        {
            ws.accept_ex(req, d);
        }

        template<class NextLayer,
            class Buffers, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req,
                Buffers const& buffers,
                    Decorator const& d) const
        {
            ws.accept_ex(req, buffers, d);
        }

        template<class NextLayer>
        void
        handshake(stream<NextLayer>& ws,
            string_view uri,
                string_view path) const
        {
            ws.handshake(uri, path);
        }

        template<class NextLayer>
        void
        handshake(stream<NextLayer>& ws,
            response_type& res,
                string_view uri,
                    string_view path) const
        {
            ws.handshake(res, uri, path);
        }

        template<class NextLayer, class Decorator>
        void
        handshake_ex(stream<NextLayer>& ws,
            string_view uri,
                string_view path,
                    Decorator const& d) const
        {
            ws.handshake_ex(uri, path, d);
        }

        template<class NextLayer, class Decorator>
        void
        handshake_ex(stream<NextLayer>& ws,
            response_type& res,
                string_view uri,
                    string_view path,
                        Decorator const& d) const
        {
            ws.handshake_ex(res, uri, path, d);
        }

        template<class NextLayer>
        void
        ping(stream<NextLayer>& ws,
            ping_data const& payload) const
        {
            ws.ping(payload);
        }

        template<class NextLayer>
        void
        pong(stream<NextLayer>& ws,
            ping_data const& payload) const
        {
            ws.pong(payload);
        }

        template<class NextLayer>
        void
        close(stream<NextLayer>& ws,
            close_reason const& cr) const
        {
            ws.close(cr);
        }

        template<
            class NextLayer, class DynamicBuffer>
        std::size_t
        read(stream<NextLayer>& ws,
            DynamicBuffer& buffer) const
        {
            return ws.read(buffer);
        }

        template<
            class NextLayer, class DynamicBuffer>
        std::size_t
        read_some(stream<NextLayer>& ws,
            std::size_t limit,
            DynamicBuffer& buffer) const
        {
            return ws.read_some(buffer, limit);
        }

        template<
            class NextLayer, class MutableBufferSequence>
        std::size_t
        read_some(stream<NextLayer>& ws,
            MutableBufferSequence const& buffers) const
        {
            return ws.read_some(buffers);
        }

        template<
            class NextLayer, class ConstBufferSequence>
        std::size_t
        write(stream<NextLayer>& ws,
            ConstBufferSequence const& buffers) const
        {
            return ws.write(buffers);
        }

        template<
            class NextLayer, class ConstBufferSequence>
        std::size_t
        write_some(stream<NextLayer>& ws, bool fin,
            ConstBufferSequence const& buffers) const
        {
            return ws.write_some(fin, buffers);
        }

        template<
            class NextLayer, class ConstBufferSequence>
        std::size_t
        write_raw(stream<NextLayer>& ws,
            ConstBufferSequence const& buffers) const
        {
            return boost::asio::write(
                ws.next_layer(), buffers);
        }
    };

    //--------------------------------------------------------------------------

    class AsyncClient
    {
        boost::asio::yield_context& yield_;

    public:
        explicit
        AsyncClient(boost::asio::yield_context& yield)
            : yield_(yield)
        {
        }

        template<class NextLayer>
        void
        accept(stream<NextLayer>& ws) const
        {
            error_code ec;
            ws.async_accept(yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, class Buffers>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept(stream<NextLayer>& ws,
            Buffers const& buffers) const
        {
            error_code ec;
            ws.async_accept(buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        accept(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req) const
        {
            error_code ec;
            ws.async_accept(req, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer,
            class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer,
            class Buffers, class Decorator>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept_ex(stream<NextLayer>& ws,
            Buffers const& buffers,
                Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(buffers, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req,
                Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(req, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer,
            class Buffers, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
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

        template<class NextLayer>
        void
        handshake(stream<NextLayer>& ws,
            string_view uri,
                string_view path) const
        {
            error_code ec;
            ws.async_handshake(
                uri, path, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        handshake(stream<NextLayer>& ws,
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

        template<class NextLayer, class Decorator>
        void
        handshake_ex(stream<NextLayer>& ws,
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

        template<class NextLayer, class Decorator>
        void
        handshake_ex(stream<NextLayer>& ws,
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

        template<class NextLayer>
        void
        ping(stream<NextLayer>& ws,
            ping_data const& payload) const
        {
            error_code ec;
            ws.async_ping(payload, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        pong(stream<NextLayer>& ws,
            ping_data const& payload) const
        {
            error_code ec;
            ws.async_pong(payload, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        close(stream<NextLayer>& ws,
            close_reason const& cr) const
        {
            error_code ec;
            ws.async_close(cr, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, class DynamicBuffer>
        std::size_t
        read(stream<NextLayer>& ws,
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
            class NextLayer, class DynamicBuffer>
        std::size_t
        read_some(stream<NextLayer>& ws,
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
            class NextLayer, class MutableBufferSequence>
        std::size_t
        read_some(stream<NextLayer>& ws,
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
            class NextLayer, class ConstBufferSequence>
        std::size_t
        write(stream<NextLayer>& ws,
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
            class NextLayer, class ConstBufferSequence>
        std::size_t
        write_some(stream<NextLayer>& ws, bool fin,
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
            class NextLayer, class ConstBufferSequence>
        std::size_t
        write_raw(stream<NextLayer>& ws,
            ConstBufferSequence const& buffers) const
        {
            error_code ec;
            auto const bytes_transferred =
                boost::asio::async_write(
                    ws.next_layer(), buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_transferred;
        }
    };
};

} // websocket
} // beast
} // boost

#endif
