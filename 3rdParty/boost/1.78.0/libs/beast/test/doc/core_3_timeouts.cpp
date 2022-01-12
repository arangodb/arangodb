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
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/spawn.hpp>
#include <cstdlib>
#include <utility>
#include <string>

namespace boost {
namespace beast {

namespace {

struct handler_type
{
    template<class... Args>
    void operator()(Args&&...)
    {
    }
};

void
core_3_timeouts_snippets()
{
    handler_type handler;

    #include "snippets.ipp"

    {
    //[code_core_3_timeouts_1

        // `ioc` will be used to dispatch completion handlers
        tcp_stream stream(ioc);

    //]
    }

    {
    //[code_core_3_timeouts_2

        // The resolver is used to look up the IP addresses for a domain name
        net::ip::tcp::resolver resolver(ioc);

        // The stream will use the same executor as the resolver
        tcp_stream stream(resolver.get_executor());

    //]
    }

    {
    //[code_core_3_timeouts_3

        // The strand will be used to invoke all completion handlers
        tcp_stream stream(net::make_strand(ioc));

    //]

        net::ip::tcp::resolver resolver(ioc);

    //[code_core_3_timeouts_4

        // Set the logical operation timer to 30 seconds
        stream.expires_after (std::chrono::seconds(30));

        // If the connection is not established within 30 seconds,
        // the operation will be canceled and the handler will receive
        // error::timeout as the error code.

        stream.async_connect(resolver.resolve("www.example.com", "http"),
            [](error_code ec, net::ip::tcp::endpoint ep)
            {
                if(ec == error::timeout)
                    std::cerr << "async_connect took too long\n";
                else if(! ec)
                    std::cout << "Connected to " << ep << "\n";
            }
        );

        // The timer is still running. If we don't want the next
        // operation to time out 30 seconds relative to the previous
        // call  to `expires_after`, we need to turn it off before
        // starting another asynchronous operation.

        stream.expires_never();

    //]
    }

    {
    //[code_core_3_timeouts_5

        // The acceptor is used to listen and accept incoming connections.
        // We construct the acceptor to use a new strand, and listen
        // on the loopback address with an operating-system assigned port.

        net::ip::tcp::acceptor acceptor(net::make_strand(ioc));
        acceptor.bind(net::ip::tcp::endpoint(net::ip::make_address_v4("127.0.0.1"), 0));
        acceptor.listen(0);

        // This blocks until a new incoming connection is established.
        // Upon success, the function returns a new socket which is
        // connected to the peer. The socket will have its own executor,
        // which in the call below is a new strand for the I/O context.

        net::ip::tcp::socket s = acceptor.accept(net::make_strand(ioc));

        // Construct a new tcp_stream from the connected socket.
        // The stream will use the strand created when the connection
        // was accepted.

        tcp_stream stream(std::move(s));
    //]
    }

    {
        tcp_stream stream(ioc);

    //[code_core_3_timeouts_6

        std::string s;

        // Set the logical operation timer to 30 seconds.
        stream.expires_after (std::chrono::seconds(30));

        // Read a line from the stream into the string.
        net::async_read_until(stream, net::dynamic_buffer(s), '\n',
            [&s, &stream](error_code ec, std::size_t bytes_transferred)
            {
                if(ec)
                    return;

                // read_until can read past the '\n', these will end up in
                // our buffer but we don't want to echo those extra received
                // bytes. `bytes_transferred` will be the number of bytes
                // up to and including the '\n'. We use `buffers_prefix` so
                // that extra data is not written.

                net::async_write(stream, buffers_prefix(bytes_transferred, net::buffer(s)),
                    [&s](error_code ec, std::size_t bytes_transferred)
                    {
                        // Consume the line from the buffer
                        s.erase(s.begin(), s.begin() + bytes_transferred);

                        if(ec)
                            std::cerr << "Error: " << ec.message() << "\n";
                    });
            });
    //]
    }

    {
        tcp_stream stream(ioc);

    //[code_core_3_timeouts_7

        std::string s1;
        std::string s2;

        // Set the logical operation timer to 15 seconds.
        stream.expires_after (std::chrono::seconds(15));

        // Read another line from the stream into our dynamic buffer.
        // The operation will time out after 15 seconds.

        net::async_read_until(stream, net::dynamic_buffer(s1), '\n', handler);

        // Set the logical operation timer to 30 seconds.
        stream.expires_after (std::chrono::seconds(30));

        // Write the contents of the other buffer.
        // This operation will time out after 30 seconds.

        net::async_write(stream, net::buffer(s2), handler);

    //]
    }

    {
    //[code_core_3_timeouts_8

        // To declare a stream with a rate policy, it is necessary to
        // write out all of the template parameter types.
        //
        // `simple_rate_policy` is default constructible, but
        // if the choice of RatePolicy is not DefaultConstructible,
        // an instance of the type may be passed to the constructor.

        basic_stream<net::ip::tcp, net::any_io_executor, simple_rate_policy> stream(ioc);

        // The policy object, which is default constructed, or
        // decay-copied upon construction, is attached to the stream
        // and may be accessed through the function `rate_policy`.
        //
        // Here we set individual rate limits for reading and writing

        stream.rate_policy().read_limit(10000); // bytes per second

        stream.rate_policy().write_limit(850000); // bytes per second
    //]
    }
}

//[code_core_3_timeouts_1f

/** This function echoes back received lines from a peer, with a timeout.

    The algorithm terminates upon any error (including timeout).
*/
template <class Protocol, class Executor>
void do_async_echo (basic_stream<Protocol, Executor>& stream)
{
    // This object will hold our state when reading the line.

    struct echo_line
    {
        basic_stream<Protocol, Executor>& stream;

        // The shared pointer is used to extend the lifetime of the
        // string until the last asynchronous operation completes.
        std::shared_ptr<std::string> s;

        // This starts a new operation to read and echo a line
        void operator()()
        {
            // If a line is not sent and received within 30 seconds, then
            // the connection will be closed and this algorithm will terminate.

            stream.expires_after(std::chrono::seconds(30));

            // Read a line from the stream into our dynamic buffer, with a timeout
            net::async_read_until(stream, net::dynamic_buffer(*s), '\n', std::move(*this));
        }

        // This function is called when the read completes
        void operator()(error_code ec, std::size_t bytes_transferred)
        {
            if(ec)
                return;

            net::async_write(stream, buffers_prefix(bytes_transferred, net::buffer(*s)),
                [this](error_code ec, std::size_t bytes_transferred)
                {
                    s->erase(s->begin(), s->begin() + bytes_transferred);

                    if(! ec)
                    {
                        // Run this algorithm again
                        echo_line{stream, std::move(s)}();
                    }
                    else
                    {
                        std::cerr << "Error: " << ec.message() << "\n";
                    }
                });
        }
    };

    // Create the operation and run it
    echo_line{stream, std::make_shared<std::string>()}();
}

//]

//[code_core_3_timeouts_2f

/** Request an HTTP resource from a TLS host and return it as a string, with a timeout.

    This example uses fibers (stackful coroutines) and its own I/O context.
*/
std::string
https_get (std::string const& host, std::string const& target, error_code& ec)
{
    // It is the responsibility of the algorithm to clear the error first.
    ec = {};

    // We use our own I/O context, to make this function blocking.
    net::io_context ioc;

    // This context is used to hold client and server certificates.
    // We do not perform certificate verification in this example.

    net::ssl::context ctx(net::ssl::context::tlsv12);

    // This string will hold the body of the HTTP response, if any.
    std::string result;

    // Note that Networking TS does not come with spawn. This function
    // launches a "fiber" which is a coroutine that has its own separately
    // allocated stack.

    boost::asio::spawn(ioc,
    [&](boost::asio::yield_context yield)
    {
        // We use the Beast ssl_stream wrapped around a beast tcp_stream.
        ssl_stream<tcp_stream> stream(ioc, ctx);

        // The resolver will be used to look up the IP addresses for the host name
        net::ip::tcp::resolver resolver(ioc);

        // First, look up the name. Networking has its own timeout for this.
        // The `yield` object is a CompletionToken which specializes the
        // `net::async_result` customization point to make the fiber work.
        //
        // This call will appear to "block" until the operation completes.
        // It isn't really blocking. Instead, the fiber implementation saves
        // the call stack and suspends the function until the asynchronous
        // operation is complete. Then it restores the call stack, and resumes
        // the function to the statement following the async_resolve. This
        // allows an asynchronous algorithm to be expressed synchronously.

        auto const endpoints = resolver.async_resolve(host, "https", {}, yield[ec]);
        if(ec)
            return;

        // The function `get_lowest_layer` retrieves the "bottom most" object
        // in the stack of stream layers. In this case it will be the tcp_stream.
        // This timeout will apply to all subsequent operations collectively.
        // That is to say, they must all complete within the same 30 second
        // window.

        get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

        // `tcp_stream` range connect algorithms are member functions, unlike net::
        get_lowest_layer(stream).async_connect(endpoints, yield[ec]);
        if(ec)
            return;

        // Perform the TLS handshake
        stream.async_handshake(net::ssl::stream_base::client, yield[ec]);
        if(ec)
            return;

        // Send an HTTP GET request for the target
        {
            http::request<http::empty_body> req;
            req.method(http::verb::get);
            req.target(target);
            req.version(11);
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "Beast");
            http::async_write(stream, req, yield[ec]);
            if(ec)
                return;
        }

        // Now read the response
        flat_buffer buffer;
        http::response<http::string_body> res;
        http::async_read(stream, buffer, res, yield[ec]);
        if(ec)
            return;

        // Try to perform the TLS shutdown handshake
        stream.async_shutdown(yield[ec]);

        // `net::ssl::error::stream_truncated`, also known as an SSL "short read",
        // indicates the peer closed the connection without performing the
        // required closing handshake (for example, Google does this to
        // improve performance). Generally this can be a security issue,
        // but if your communication protocol is self-terminated (as
        // it is with both HTTP and WebSocket) then you may simply
        // ignore the lack of close_notify:
        //
        // https://github.com/boostorg/beast/issues/38
        //
        // https://security.stackexchange.com/questions/91435/how-to-handle-a-malicious-ssl-tls-shutdown
        //
        // When a short read would cut off the end of an HTTP message,
        // Beast returns the error beast::http::error::partial_message.
        // Therefore, if we see a short read here, it has occurred
        // after the message has been completed, so it is safe to ignore it.

        if(ec == net::ssl::error::stream_truncated)
            ec = {};
        else if(ec)
            return;

        // Set the string to return to the caller
        result = std::move(res.body());
    });

    // `run` will dispatch completion handlers, and block until there is
    // no more "work" remaining. When this call returns, the operations
    // are complete and we can give the caller the result.
    ioc.run();

    return result;
}

//]

//[code_core_3_timeouts_3f

class window
{
    std::size_t value_ = 0;

    // The size of the exponential window, in seconds.
    // This should be a power of two.

    static std::size_t constexpr Window = 4;

public:
    /** Returns the number of elapsed seconds since the given time, and adjusts the time.

        This function returns the number of elapsed seconds since the
        specified time point, rounding down. It also moves the specified
        time point forward by the number of elapsed seconds.

        @param since The time point from which to calculate elapsed time.
        The function will modify the value, by adding the number of elapsed
        seconds to it.

        @return The number of elapsed seconds.
    */
    template<class Clock, class Duration>
    static
    std::chrono::seconds
    get_elapsed(std::chrono::time_point<Clock, Duration>& since) noexcept
    {
        auto const elapsed = std::chrono::duration_cast<
            std::chrono::seconds>(Clock::now() - since);
        since += elapsed;
        return elapsed;
    }

    /// Returns the current value, after adding the given sample.
    std::size_t
    update(std::size_t sample, std::chrono::seconds elapsed) noexcept
    {
        // Apply exponential decay.
        //
        // This formula is fast (no division or multiplication) but inaccurate.
        // It overshoots by `n*(1-a)/(1-a^n), where a=(window-1)/window`.
        // Could be good enough for a rough approximation, but if relying
        // on this for production please perform tests!

        auto count = elapsed.count();
        while(count--)
            value_ -= (value_ + Window - 1) / Window;
        value_ += sample;
        return value_ / Window;
    }
    /// Returns the current value
    std::size_t
    value() const noexcept
    {
        return value_ / Window;
    }
};

//]

//[code_core_3_timeouts_4f

/** A RatePolicy to measure instantaneous throughput.

    This measures the rate of transfer for reading and writing
    using a simple exponential decay function.
*/
class rate_gauge
{
    // The clock used to measure elapsed time
    using clock_type = std::chrono::steady_clock;

    // This implements an exponential smoothing window function.
    // The value `Seconds` is the size of the window in seconds.

    clock_type::time_point when_;
    std::size_t read_bytes_ = 0;
    std::size_t write_bytes_ = 0;
    window read_window_;
    window write_window_;

    // Friending this type allows us to mark the
    // member functions required by RatePolicy as private.
    friend class rate_policy_access;

    // Returns the number of bytes available to read currently
    // Required by RatePolicy
    std::size_t
    available_read_bytes() const noexcept
    {
        // no limit
        return (std::numeric_limits<std::size_t>::max)();
    }

    // Returns the number of bytes available to write currently
    // Required by RatePolicy
    std::size_t
    available_write_bytes() const noexcept
    {
        // no limit
        return (std::numeric_limits<std::size_t>::max)();
    }

    // Called every time bytes are read
    // Required by RatePolicy
    void
    transfer_read_bytes(std::size_t n) noexcept
    {
        // Add this to our running total of bytes read
        read_bytes_ += n;
    }

    // Called every time bytes are written
    // Required by RatePolicy
    void
    transfer_write_bytes(std::size_t n) noexcept
    {
        // Add this to our running total of bytes written
        write_bytes_ += n;
    }

    // Called approximately once per second
    // Required by RatePolicy
    void
    on_timer()
    {
        // Calculate elapsed time in seconds, and adjust our time point
        auto const elapsed = window::get_elapsed(when_);

        // Skip the update when elapsed==0,
        // otherwise the measurement will have jitter
        if(elapsed.count() == 0)
            return;

        // Add our samples and apply exponential decay
        read_window_.update(read_bytes_, elapsed);
        write_window_.update(write_bytes_, elapsed);

        // Reset our counts of bytes transferred
        read_bytes_ = 0;
        write_bytes_ = 0;
    }

public:
    rate_gauge()
        : when_(clock_type::now())
    {
    }

    /// Returns the current rate of reading in bytes per second
    std::size_t
    read_bytes_per_second() const noexcept
    {
        return read_window_.value();
    }

    /// Returns the current rate of writing in bytes per second
    std::size_t
    write_bytes_per_second() const noexcept
    {
        return write_window_.value();
    }
};

//]

void
core_3_timeouts_snippets2()
{
    #include "snippets.ipp"

    {
    //[code_core_3_timeouts_9

        // This stream will use our new rate_gauge policy
        basic_stream<net::ip::tcp, net::any_io_executor, rate_gauge> stream(ioc);

        //...

        // Print the current rates
        std::cout <<
            stream.rate_policy().read_bytes_per_second() << " bytes/second read\n" <<
            stream.rate_policy().write_bytes_per_second() << " bytes/second written\n";
    //]
    }
}

} // (anon)

template class basic_stream<net::ip::tcp, net::any_io_executor, rate_gauge>;

struct core_3_timeouts_test
    : public beast::unit_test::suite
{
    void
    testWindow()
    {
        window w;
        std::size_t v0 = w.value();
        std::size_t const N = 100000;
        for(std::size_t n = 1; n <= 2; ++n)
        {
            for(std::size_t i = 0;;++i)
            {
                auto const v = w.update(n * N, std::chrono::seconds(n));
                if(v == v0)
                {
                    BEAST_PASS();
                #if 0
                    log <<
                        "update(" << n*N << ", " << n <<
                        ") converged to " << w.value() <<
                        " in " << i << std::endl;
                #endif
                    break;
                }
                if(i > 1000)
                {
                    BEAST_FAIL();
                    break;
                }
                v0 = v;
            }
        }
    }

    void
    run() override
    {
        testWindow();

        BEAST_EXPECT(&core_3_timeouts_snippets);
        BEAST_EXPECT(&core_3_timeouts_snippets2);
        BEAST_EXPECT((&do_async_echo<net::ip::tcp, net::io_context::executor_type>));
        BEAST_EXPECT(&https_get);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,core_3_timeouts);

} // beast
} // boost
