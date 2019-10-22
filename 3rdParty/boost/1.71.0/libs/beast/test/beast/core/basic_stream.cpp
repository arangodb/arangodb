//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/basic_stream.hpp>

#include "stream_tests.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/optional.hpp>
#include <array>
#include <thread>

namespace boost {
namespace beast {

#if 0
template class basic_stream<
    net::ip::tcp,
    net::executor,
    unlimited_rate_policy>;

template class basic_stream<
    net::ip::tcp,
    net::executor,
    simple_rate_policy>;
#endif

namespace {

template<class Executor = net::io_context::executor_type>
class test_executor
{
public:
    // VFALCO These need to be atomic or something
    struct info
    {
        int dispatch = 0;
        int post = 0;
        int defer = 0;
        int work = 0;
        int total = 0;
    };

private:
    struct state
    {
        Executor ex;
        info info_;

        state(Executor const& ex_)
            : ex(ex_)
        {
        }
    };

    std::shared_ptr<state> sp_;

public:
    test_executor(test_executor const&) = default;
    test_executor& operator=(test_executor const&) = default;

    explicit
    test_executor(Executor const& ex)
        : sp_(std::make_shared<state>(ex))
    {
    }

    decltype(sp_->ex.context())
    context() const noexcept
    {
        return sp_->ex.context();
    }

    info&
    operator*() noexcept
    {
        return sp_->info_;
    }

    info*
    operator->() noexcept
    {
        return &sp_->info_;
    }

    void
    on_work_started() const noexcept
    {
        ++sp_->info_.work;
    }

    void
    on_work_finished() const noexcept
    {
    }

    template<class F, class A>
    void
    dispatch(F&& f, A const& a)
    {
        ++sp_->info_.dispatch;
        ++sp_->info_.total;
        sp_->ex.dispatch(
            std::forward<F>(f), a);
    }

    template<class F, class A>
    void
    post(F&& f, A const& a)
    {
        ++sp_->info_.post;
        ++sp_->info_.total;
        sp_->ex.post(
            std::forward<F>(f), a);
    }

    template<class F, class A>
    void
    defer(F&& f, A const& a)
    {
        ++sp_->info_.defer;
        ++sp_->info_.total;
        sp_->ex.defer(
            std::forward<F>(f), a);
    }
};

struct test_acceptor
{
    net::io_context ioc;
    net::ip::tcp::acceptor a;
    net::ip::tcp::endpoint ep;

    test_acceptor()
        : a(ioc)
        , ep(net::ip::make_address_v4("127.0.0.1"), 0)
    {
        a.open(ep.protocol());
        a.set_option(
            net::socket_base::reuse_address(true));
        a.bind(ep);
        a.listen(net::socket_base::max_listen_connections);
        ep = a.local_endpoint();
        a.async_accept(
            [](error_code, net::ip::tcp::socket)
            {
            });
    }
};

class test_server
{
    string_view s_;
    std::ostream& log_;
    net::io_context ioc_;
    net::ip::tcp::acceptor acceptor_;
    net::ip::tcp::socket socket_;
    std::thread t_;

    void
    fail(error_code const& ec, string_view what)
    {
        if(ec != net::error::operation_aborted)
            log_ << what << ": " << ec.message() << "\n";
    }

public:
    test_server(
        string_view s,
        net::ip::tcp::endpoint ep,
        std::ostream& log)
        : s_(s)
        , log_(log)
        , ioc_(1)
        , acceptor_(ioc_)
        , socket_(ioc_)
    {
        boost::system::error_code ec;

        acceptor_.open(ep.protocol(), ec);
        if(ec)
        {
            fail(ec, "open");
            return;
        }

        acceptor_.set_option(
            net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }

        acceptor_.bind(ep, ec);
        if(ec)
        {
            fail(ec, "bind");
            return;
        }

        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, "listen");
            return;
        }

        acceptor_.async_accept(socket_,
            [this](error_code ec)
            {
                this->on_accept(ec);
            });

        t_ = std::thread(
            [this]
            {
                ioc_.run();
            });
    }

    ~test_server()
    {
        ioc_.stop();
        t_.join();
    }

    net::ip::tcp::endpoint
    local_endpoint() const noexcept
    {
        return acceptor_.local_endpoint();
    }

private:
    class session
        : public std::enable_shared_from_this<session>
    {
        string_view s_;
        net::ip::tcp::socket socket_;
        
    public:
        session(
            string_view s,
            net::ip::tcp::socket sock,
            std::ostream&)
            : s_(s)
            , socket_(std::move(sock))
        {
        }

        void
        run()
        {
            if(s_.empty())
                socket_.async_wait(
                    net::socket_base::wait_read,
                    bind_front_handler(
                        &session::on_read,
                        shared_from_this()));
            else
                net::async_write(
                    socket_,
                    net::const_buffer(s_.data(), s_.size()),
                    bind_front_handler(
                        &session::on_write,
                        shared_from_this()));
        }

    protected:
        void
        on_read(error_code const&)
        {
        }

        void
        on_write(error_code const&, std::size_t)
        {
        }
    };

    void
    on_accept(error_code const& ec)
    {
        if(! acceptor_.is_open())
            return;
        if(ec)
            fail(ec, "accept");
        else
            std::make_shared<session>(
                s_, std::move(socket_), log_)->run();
        acceptor_.async_accept(socket_,
            [this](error_code ec)
            {
                this->on_accept(ec);
            });
    }
};

} // (anon)

class basic_stream_test
    : public beast::unit_test::suite
{
public:
    using tcp = net::ip::tcp;
    using strand = net::io_context::strand;
    using executor = net::io_context::executor_type;

    //--------------------------------------------------------------------------

    void
    testSpecialMembers()
    {
        net::io_context ioc;

        // net::io_context::executor_type

        {
            auto ex = ioc.get_executor();
            basic_stream<tcp, executor> s1(ioc);
            basic_stream<tcp, executor> s2(ex);
            basic_stream<tcp, executor> s3(ioc, tcp::v4());
            basic_stream<tcp, executor> s4(std::move(s1));
            s2.socket() =
                net::basic_stream_socket<tcp, executor>(ioc);
            BEAST_EXPECT(s1.get_executor() == ex);
            BEAST_EXPECT(s2.get_executor() == ex);
            BEAST_EXPECT(s3.get_executor() == ex);
            BEAST_EXPECT(s4.get_executor() == ex);

            BEAST_EXPECT((! static_cast<
                basic_stream<tcp, executor> const&>(
                    s2).socket().is_open()));

            test_sync_stream<
                basic_stream<
                    tcp, net::io_context::executor_type>>();

            test_async_stream<
                basic_stream<
                    tcp, net::io_context::executor_type>>();
        }

        // net::io_context::strand

        {
            auto ex = strand{ioc};
            basic_stream<tcp, strand> s1(ex);
            basic_stream<tcp, strand> s2(ex, tcp::v4());
            basic_stream<tcp, strand> s3(std::move(s1));
        #if 0
            s2.socket() = net::basic_stream_socket<
                tcp, strand>(ioc);
        #endif
            BEAST_EXPECT(s1.get_executor() == ex);
            BEAST_EXPECT(s2.get_executor() == ex);
            BEAST_EXPECT(s3.get_executor() == ex);

#if 0
            BEAST_EXPECT((! static_cast<
                basic_stream<tcp, strand> const&>(
                    s2).socket().is_open()));
#endif

            test_sync_stream<
                basic_stream<
                    tcp, net::io_context::strand>>();

            test_async_stream<
                basic_stream<
                    tcp, net::io_context::strand>>();
        }

        // construction from existing socket

#if 0
        {
            tcp::socket sock(ioc);
            basic_stream<tcp, executor> stream(std::move(sock));
        }

        {
            tcp::socket sock(ioc);
            basic_stream<tcp, strand> stream(std::move(sock));
        }
#endif

        // layers

        {
            net::socket_base::keep_alive opt;
            tcp_stream s(ioc);
            s.socket().open(tcp::v4());
            s.socket().get_option(opt);
            BEAST_EXPECT(! opt.value());
            opt = true;
            s.socket().set_option(opt);
            opt = false;
            BEAST_EXPECT(! opt.value());
        }

        // rate policies

        {
            basic_stream<tcp,
                net::io_context::executor_type,
                simple_rate_policy> s(ioc);
        }

        {
            basic_stream<tcp,
                net::io_context::executor_type,
                simple_rate_policy> s(
                    simple_rate_policy{}, ioc);
        }

        {
            basic_stream<tcp,
                net::io_context::executor_type,
                unlimited_rate_policy> s(ioc);
        }

        {
            basic_stream<tcp,
                net::io_context::executor_type,
                unlimited_rate_policy> s(
                    unlimited_rate_policy{}, ioc);
        }
    }

    class handler
    {
        boost::optional<error_code> ec_;
        std::size_t n_;

    public:
        handler(error_code ec, std::size_t n)
            : ec_(ec)
            , n_(n)
        {
        }

        handler(handler&& other)
            : ec_(other.ec_)
            , n_(boost::exchange(other.n_,
                (std::numeric_limits<std::size_t>::max)()))
        {
        }

        ~handler()
        {
            BEAST_EXPECT(
                n_ == (std::numeric_limits<std::size_t>::max)());
        }

        void
        operator()(error_code const& ec, std::size_t n)
        {
            BEAST_EXPECTS(ec == ec_, ec.message());
            BEAST_EXPECT(n == n_);
            n_ = (std::numeric_limits<std::size_t>::max)();
        }
    };

    void
    testRead()
    {
        using stream_type = basic_stream<tcp,
            net::io_context::executor_type>;

        char buf[4];
        net::io_context ioc;
        std::memset(buf, 0, sizeof(buf));
        net::mutable_buffer mb(buf, sizeof(buf));
        auto const ep = net::ip::tcp::endpoint(
            net::ip::make_address("127.0.0.1"), 0);

        // read_some

        {
            error_code ec;
            stream_type s(ioc, tcp::v4());
            BEAST_EXPECT(s.read_some(net::mutable_buffer{}) == 0);
            BEAST_EXPECT(s.read_some(net::mutable_buffer{}, ec) == 0);
            BEAST_EXPECTS(! ec, ec.message());
        }

        // async_read_some

        {
            // success
            test_server srv("*", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_never();
            s.async_read_some(mb, handler({}, 1));
            ioc.run();
            ioc.restart();
        }

        {
            // success, with timeout
            test_server srv("*", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_after(std::chrono::seconds(30));
            s.async_read_some(mb, handler({}, 1));
            ioc.run();
            ioc.restart();
        }

        {
            // empty buffer
            test_server srv("*", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_never();
            s.async_read_some(
                net::mutable_buffer{}, handler({}, 0));
            ioc.run();
            ioc.restart();
        }

        {
            // empty buffer, timeout
            test_server srv("*", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_after(std::chrono::seconds(0));
            s.async_read_some(net::mutable_buffer{},
                handler(error::timeout, 0));
            ioc.run();
            ioc.restart();
        }

        {
            // expires_after
            test_server srv("", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_after(std::chrono::seconds(0));
            s.async_read_some(mb, handler(error::timeout, 0));
            ioc.run();
            ioc.restart();
        }

        {
            // expires_at
            test_server srv("", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_at(std::chrono::steady_clock::now());
            s.async_read_some(mb, handler(error::timeout, 0));
            ioc.run();
            ioc.restart();
        }

        {
            // stream destroyed
            test_server srv("", ep, log);
            {
                stream_type s(ioc);
                s.socket().connect(srv.local_endpoint());
                s.expires_after(std::chrono::seconds(0));
                s.async_read_some(mb,
                    [](error_code, std::size_t)
                    {
                    });
            }
            ioc.run();
            ioc.restart();
        }

        {
            // stale timer
            test_acceptor a;
            stream_type s(ioc);
            s.expires_after(std::chrono::milliseconds(50));
            s.async_read_some(mb,
                [](error_code, std::size_t)
                {
                });
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100));
            ioc.run();
            ioc.restart();
        }

        // abandoned operation
        {
            stream_type s(ioc);
            s.async_read_some(net::mutable_buffer{},
                [](error_code, std::size_t)
                {
                    BEAST_FAIL();
                });
        }
    }

    void
    testWrite()
    {
        using stream_type = basic_stream<tcp,
            net::io_context::executor_type>;

        char buf[4];
        net::io_context ioc;
        std::memset(buf, 0, sizeof(buf));
        net::const_buffer cb(buf, sizeof(buf));
        auto const ep = net::ip::tcp::endpoint(
            net::ip::make_address("127.0.0.1"), 0);

        // write_some

        {
            error_code ec;
            stream_type s(ioc, tcp::v4());
            BEAST_EXPECT(s.write_some(net::const_buffer{}) == 0);
            BEAST_EXPECT(s.write_some(net::const_buffer{}, ec) == 0);
            BEAST_EXPECTS(! ec, ec.message());
        }

        // async_write_some

        {
            // success
            test_server srv("*", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_never();
            s.async_write_some(cb, handler({}, 4));
            ioc.run();
            ioc.restart();
        }

        {
            // success, with timeout
            test_server srv("*", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_after(std::chrono::seconds(30));
            s.async_write_some(cb, handler({}, 4));
            ioc.run();
            ioc.restart();
        }

        {
            // empty buffer
            test_server srv("*", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_never();
            s.async_write_some(
                net::const_buffer{}, handler({}, 0));
            ioc.run();
            ioc.restart();
        }

        {
            // empty buffer, timeout
            test_server srv("*", ep, log);
            stream_type s(ioc);
            s.socket().connect(srv.local_endpoint());
            s.expires_after(std::chrono::seconds(0));
            s.async_write_some(net::const_buffer{},
                handler(error::timeout, 0));
            ioc.run();
            ioc.restart();
        }

        // abandoned operation
        {
            stream_type s(ioc);
            s.async_write_some(cb,
                [](error_code, std::size_t)
                {
                    BEAST_FAIL();
                });
        }
    }

    void
    testConnect()
    {
        using stream_type = basic_stream<tcp,
            net::io_context::executor_type>;

        struct range
        {
            tcp::endpoint ep;

            using iterator =
                tcp::endpoint const*;

            // VFALCO This is here because asio mistakenly requires it
            using const_iterator =
                tcp::endpoint const*;

            iterator begin() const noexcept
            {
                return &ep;
            }

            // VFALCO need to use const_iterator to silence
            //        warning about unused types
            const_iterator end() const noexcept
            {
                return begin() + 1;
            }
        };

        class connect_handler
        {
            bool pass_ = false;
            boost::optional<error_code> expected_ = {};

        public:
            ~connect_handler()
            {
                BEAST_EXPECT(pass_);
            }

            connect_handler()
                : expected_(error_code{})
            {
            }

            explicit
            connect_handler(error_code expected)
                : expected_(expected)
            {
            }

            explicit
            connect_handler(boost::none_t)
            {
            }

            connect_handler(connect_handler&& other)
                : pass_(boost::exchange(other.pass_, true))
                , expected_(other.expected_)
            {
            }

            void operator()(error_code ec)
            {
                pass_ = true;
                if(expected_)
                    BEAST_EXPECTS(
                        ec == expected_, ec.message());
            }
        };

        struct range_handler
        {
            bool pass = false;

            range_handler() = default;

            range_handler(range_handler&& other)
                : pass(boost::exchange(other.pass, true))
            {
            }

            ~range_handler()
            {
                BEAST_EXPECT(pass);
            }

            void operator()(error_code ec, tcp::endpoint)
            {
                pass = true;
                BEAST_EXPECTS(! ec, ec.message());
            }
        };

        struct iterator_handler
        {
            bool pass = false;

            iterator_handler() = default;

            iterator_handler(iterator_handler&& other)
                : pass(boost::exchange(other.pass, true))
            {
            }

            ~iterator_handler()
            {
                BEAST_EXPECT(pass);
            }

            void operator()(error_code ec, tcp::endpoint const*)
            {
                pass = true;
                BEAST_EXPECTS(! ec, ec.message());
            }
        };

        struct connect_condition
        {
            bool operator()(error_code, tcp::endpoint) const
            {
                return true;
            };
        };

        range r;
        net::io_context ioc;
        connect_condition cond;

        // connect (member)

        {
            test_acceptor a;
            stream_type s(ioc);
            error_code ec;
            s.connect(a.ep);
            s.socket().close();
            s.connect(a.ep, ec);
            BEAST_EXPECTS(! ec, ec.message());
        }

        // connect

        {
            test_acceptor a;
            stream_type s(ioc);
            error_code ec;
            r.ep = a.ep;
            s.connect(r);
            s.socket().close();
            s.connect(r, ec);
            BEAST_EXPECTS(! ec, ec.message());
        }

        {
            test_acceptor a;
            stream_type s(ioc);
            error_code ec;
            r.ep = a.ep;
            s.connect(r, cond);
            s.socket().close();
            s.connect(r, cond, ec);
            BEAST_EXPECTS(! ec, ec.message());
        }

        {
            test_acceptor a;
            stream_type s(ioc);
            error_code ec;
            r.ep = a.ep;
            s.connect(r.begin(), r.end());
            s.socket().close();
            s.connect(r.begin(), r.end(), ec);
            BEAST_EXPECTS(! ec, ec.message());
        }

        {
            test_acceptor a;
            stream_type s(ioc);
            error_code ec;
            r.ep = a.ep;
            s.connect(r.begin(), r.end(), cond);
            s.socket().close();
            s.connect(r.begin(), r.end(), cond, ec);
            BEAST_EXPECTS(! ec, ec.message());
        }

        // async_connect (member)

        {
            test_acceptor a;
            stream_type s(ioc);
            s.expires_never();
            s.async_connect(a.ep, connect_handler{});
            ioc.run();
            ioc.restart();
            s.socket().close();
            s.expires_after(std::chrono::seconds(30));
            s.async_connect(a.ep, connect_handler{});
            ioc.run();
            ioc.restart();
        }

        // async_connect

        {
            test_acceptor a;
            stream_type s(ioc);
            r.ep = a.ep;
            s.expires_never();
            s.async_connect(r, range_handler{});
            ioc.run();
            ioc.restart();
            s.socket().close();
            s.expires_after(std::chrono::seconds(30));
            s.async_connect(r, range_handler{});
            ioc.run();
            ioc.restart();
        }

        {
            test_acceptor a;
            stream_type s(ioc);
            r.ep = a.ep;
            s.expires_never();
            s.async_connect(r, cond, range_handler{});
            ioc.run();
            ioc.restart();
            s.socket().close();
            s.expires_after(std::chrono::seconds(30));
            s.async_connect(r, cond, range_handler{});
            ioc.run();
            ioc.restart();
        }

        {
            test_acceptor a;
            stream_type s(ioc);
            r.ep = a.ep;
            s.expires_never();
            s.async_connect(r.begin(), r.end(),
                iterator_handler{});
            ioc.run();
            ioc.restart();
            s.socket().close();
            s.expires_after(std::chrono::seconds(30));
            s.async_connect(r.begin(), r.end(),
                iterator_handler{});
            ioc.run();
            ioc.restart();
        }

        {
            test_acceptor a;
            stream_type s(ioc);
            r.ep = a.ep;
            s.expires_never();
            s.async_connect(r.begin(), r.end(), cond,
                iterator_handler{});
            ioc.run();
            ioc.restart();
            s.socket().close();
            s.expires_after(std::chrono::seconds(30));
            s.async_connect(r.begin(), r.end(), cond,
                iterator_handler{});
            ioc.run();
            ioc.restart();
        }
#if 0
        // use_future

        BEAST_EXPECT(static_cast<std::future<
            tcp::endpoint>(*)(stream_type&,
                std::array<tcp::endpoint, 2> const&,
                net::use_future_t<>&&)>(
                    &beast::async_connect));

        BEAST_EXPECT(static_cast<std::future<
            tcp::endpoint>(*)(stream_type&,
                std::array<tcp::endpoint, 2> const&,
                connect_condition const&,
                net::use_future_t<>&&)>(
                    &beast::async_connect));

        BEAST_EXPECT(static_cast<std::future<
            tcp::endpoint const*>(*)(stream_type&,
                tcp::endpoint const*,
                tcp::endpoint const*,
                net::use_future_t<>&&)>(
                    &beast::async_connect));

        BEAST_EXPECT(static_cast<std::future<
            tcp::endpoint const*>(*)(stream_type&,
                tcp::endpoint const*,
                tcp::endpoint const*,
                connect_condition const&,
                net::use_future_t<>&&)>(
                    &beast::async_connect));

        // yield_context

        BEAST_EXPECT(static_cast<
            tcp::endpoint(*)(stream_type&,
                std::array<tcp::endpoint, 2> const&,
                net::yield_context&&)>(
                    &beast::async_connect));

        BEAST_EXPECT(static_cast<
            tcp::endpoint(*)(stream_type&,
                std::array<tcp::endpoint, 2> const&,
                connect_condition const&,
                net::yield_context&&)>(
                    &beast::async_connect));

        BEAST_EXPECT(static_cast<
            tcp::endpoint const*(*)(stream_type&,
                tcp::endpoint const*,
                tcp::endpoint const*,
                net::yield_context&&)>(
                    &beast::async_connect));

        BEAST_EXPECT(static_cast<
            tcp::endpoint const*(*)(stream_type&,
                tcp::endpoint const*,
                tcp::endpoint const*,
                connect_condition const&,
                net::yield_context&&)>(
                    &beast::async_connect));
#endif

        //
        // async_connect timeout
        //

        {
            // normal timeout
            // Requires timeout happen before ECONNREFUSED 
            stream_type s(ioc);
            auto const ep = net::ip::tcp::endpoint(
            #if 1
                // This address _should_ be unconnectible
                net::ip::make_address("72.5.65.111"), 1);
            #else
                // On Travis ECONNREFUSED happens before the timeout
                net::ip::make_address("127.0.0.1"), 1);
            #endif
            s.expires_after(std::chrono::seconds(0));
            s.async_connect(ep, connect_handler{error::timeout});
            ioc.run_for(std::chrono::seconds(1));
            ioc.restart();
        }

        {
            // stream destroyed
            {
                stream_type s(ioc);
                auto const ep = net::ip::tcp::endpoint(
                    net::ip::make_address("127.0.0.1"), 1);
                s.expires_after(std::chrono::seconds(0));
                s.async_connect(ep, connect_handler{boost::none});
            }
            ioc.run();
            ioc.restart();
        }

        {
            // stale timer
            test_acceptor a;
            stream_type s(ioc);
            s.expires_after(std::chrono::milliseconds(50));
            s.async_connect(a.ep, connect_handler{});
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100));
            ioc.run();
            ioc.restart();
        }

        // abandoned operation
        {
            stream_type s(ioc);
            net::ip::tcp::endpoint ep(
                net::ip::make_address_v4("127.0.0.1"), 1);
            s.async_connect(ep,
                [](error_code)
                {
                    BEAST_FAIL();
                });
        }
    }

    void
    testMembers()
    {
        using stream_type = basic_stream<tcp,
            net::io_context::executor_type>;

        class handler
        {
            bool pass_ = false;
            boost::optional<error_code> expected_ = {};

        public:
            ~handler()
            {
                BEAST_EXPECT(pass_);
            }

            handler()
                : expected_(error_code{})
            {
            }

            explicit
            handler(error_code expected)
                : expected_(expected)
            {
            }

            explicit
            handler(boost::none_t)
            {
            }

            handler(handler&& other)
                : pass_(boost::exchange(other.pass_, true))
                , expected_(other.expected_)
            {
            }

            void operator()(error_code ec, std::size_t)
            {
                pass_ = true;
                if(expected_)
                    BEAST_EXPECTS(
                        ec == expected_, ec.message());
            }
        };

        auto const ep = net::ip::tcp::endpoint(
            net::ip::make_address("127.0.0.1"), 0);

        char buf[4];
        net::io_context ioc;
        auto mb = net::buffer(buf);
        std::memset(buf, 0, sizeof(buf));

        // cancel

        {
            test_server srv("", ep, log);
            stream_type s(ioc);
            s.connect(srv.local_endpoint());
            s.expires_never();
            s.socket().async_read_some(mb, handler(
                net::error::operation_aborted));
            s.cancel();
            ioc.run();
            ioc.restart();
        }

        // close

        {
            test_server srv("", ep, log);
            stream_type s(ioc);
            s.connect(srv.local_endpoint());
            s.expires_never();
            s.socket().async_read_some(mb,
                handler(boost::none));
            s.close();
            ioc.run();
            ioc.restart();
        }

        // destructor

        {
            test_server srv("", ep, log);
            {
                stream_type s(ioc);
                s.connect(srv.local_endpoint());
                s.expires_never();
                s.socket().async_read_some(mb,
                    handler(boost::none));
            }
            ioc.run();
            ioc.restart();
        }

        // customization points

        {
            stream_type s(ioc);
            beast::close_socket(s);
        }

        {
            error_code ec;
            stream_type s(ioc);
            teardown(role_type::client, s, ec);
        }

        {
            stream_type s(ioc);
            async_teardown(role_type::server, s,
                [](error_code)
                {
                });
        }
    }

    //--------------------------------------------------------------------------

    http::response<http::string_body>
    make_response(http::request<http::empty_body>)
    {
        return {};
    }
    void process_http_1 (tcp_stream& stream, net::yield_context yield)
    {
        flat_buffer buffer;
        http::request<http::empty_body> req;

        // Read the request, with a 15 second timeout
        stream.expires_after(std::chrono::seconds(15));
        http::async_read(stream, buffer, req, yield);

        // Calculate the response
        http::response<http::string_body> res = make_response(req);

        // Send the response, with a 30 second timeout.
        stream.expires_after (std::chrono::seconds(30));
        http::async_write (stream, res, yield);
    }

    void process_http_2 (tcp_stream& stream, net::yield_context yield)
    {
        flat_buffer buffer;
        http::request<http::empty_body> req;

        // Require that the read and write combined take no longer than 30 seconds
        stream.expires_after(std::chrono::seconds(30));

        http::async_read(stream, buffer, req, yield);

        http::response<http::string_body> res = make_response(req);
        http::async_write (stream, res, yield);
    }

    void
    testJavadocs()
    {
        BEAST_EXPECT(&basic_stream_test::process_http_1);
        BEAST_EXPECT(&basic_stream_test::process_http_2);
    }

    //--------------------------------------------------------------------------

    void
    run()
    {
        testSpecialMembers();
        testRead();
        testWrite();
        testConnect();
        testMembers();
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,basic_stream);

} // beast
} // boost
