//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP flex server (plain and SSL), asynchronous
//
//------------------------------------------------------------------------------

#include "example/common/detect_ssl.hpp"
#include "example/common/server_certificate.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view
mime_type(boost::beast::string_view path)
{
    using boost::beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == boost::beast::string_view::npos)
            return boost::beast::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    boost::beast::string_view base,
    boost::beast::string_view path)
{
    if(base.empty())
        return path.to_string();
    std::string result = base.to_string();
#if BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
void
handle_request(
    boost::beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
{
    // Returns a bad request response
    auto const bad_request =
    [&req](boost::beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](boost::beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
    [&req](boost::beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + what.to_string() + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if( req.method() != http::verb::get &&
        req.method() != http::verb::head)
        return send(bad_request("Unknown HTTP-method"));

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != boost::beast::string_view::npos)
        return send(bad_request("Illegal request-target"));

    // Build the path to the requested file
    std::string path = path_cat(doc_root, req.target());
    if(req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    boost::beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), boost::beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if(ec == boost::system::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

    // Handle an unknown error
    if(ec)
        return send(server_error(ec.message()));

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if(req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(boost::system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection.
// This uses the Curiously Recurring Template Pattern so that
// the same code works with both SSL streams and regular sockets.
template<class Derived>
class session
{
    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda
    {
        session& self_;

        explicit
        send_lambda(session& self)
            : self_(self)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<
                http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.derived().stream(),
                *sp,
                boost::asio::bind_executor(
                    self_.strand_,
                    std::bind(
                        &session::on_write,
                        self_.derived().shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2,
                        sp->need_eof())));
        }
    };

    std::shared_ptr<std::string const> doc_root_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;

protected:
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    boost::beast::flat_buffer buffer_;

public:
    // Take ownership of the buffer
    explicit
    session(
        boost::asio::io_context& ioc,
        boost::beast::flat_buffer buffer,
        std::shared_ptr<std::string const> const& doc_root)
        : doc_root_(doc_root)
        , lambda_(*this)
        , strand_(ioc.get_executor())
        , buffer_(std::move(buffer))
    {
    }

    void
    do_read()
    {
        // Read a request
        http::async_read(
            derived().stream(),
            buffer_,
            req_,
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &session::on_read,
                    derived().shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    void
    on_read(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return derived().do_eof();

        if(ec)
            return fail(ec, "read");

        // Send the response
        handle_request(*doc_root_, std::move(req_), lambda_);
    }

    void
    on_write(
        boost::system::error_code ec,
        std::size_t bytes_transferred,
        bool close)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return derived().do_eof();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
    }
};

// Handles a plain HTTP connection
class plain_session
    : public session<plain_session>
    , public std::enable_shared_from_this<plain_session>
{
    tcp::socket socket_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;

public:
    // Create the session
    plain_session(
        tcp::socket socket,
        boost::beast::flat_buffer buffer,
        std::shared_ptr<std::string const> const& doc_root)
        : session<plain_session>(
            socket.get_executor().context(),
            std::move(buffer),
            doc_root)
        , socket_(std::move(socket))
        , strand_(socket_.get_executor())
    {
    }

    // Called by the base class
    tcp::socket&
    stream()
    {
        return socket_;
    }

    // Start the asynchronous operation
    void
    run()
    {
        do_read();
    }

    void
    do_eof()
    {
        // Send a TCP shutdown
        boost::system::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

// Handles an SSL HTTP connection
class ssl_session
    : public session<ssl_session>
    , public std::enable_shared_from_this<ssl_session>
{
    tcp::socket socket_;
    ssl::stream<tcp::socket&> stream_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;

public:
    // Create the session
    ssl_session(
        tcp::socket socket,
        ssl::context& ctx,
        boost::beast::flat_buffer buffer,
        std::shared_ptr<std::string const> const& doc_root)
        : session<ssl_session>(
            socket.get_executor().context(),
            std::move(buffer),
            doc_root)
        , socket_(std::move(socket))
        , stream_(socket_, ctx)
        , strand_(stream_.get_executor())
    {
    }

    // Called by the base class
    ssl::stream<tcp::socket&>&
    stream()
    {
        return stream_;
    }

    // Start the asynchronous operation
    void
    run()
    {
        // Perform the SSL handshake
        // Note, this is the buffered version of the handshake.
        stream_.async_handshake(
            ssl::stream_base::server,
            buffer_.data(),
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &ssl_session::on_handshake,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }
    void
    on_handshake(
        boost::system::error_code ec,
        std::size_t bytes_used)
    {
        if(ec)
            return fail(ec, "handshake");

        // Consume the portion of the buffer used by the handshake
        buffer_.consume(bytes_used);

        do_read();
    }

    void
    do_eof()
    {
        // Perform the SSL shutdown
        stream_.async_shutdown(
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &ssl_session::on_shutdown,
                    shared_from_this(),
                    std::placeholders::_1)));
    }

    void
    on_shutdown(boost::system::error_code ec)
    {
        if(ec)
            return fail(ec, "shutdown");

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Detects SSL handshakes
class detect_session : public std::enable_shared_from_this<detect_session>
{
    tcp::socket socket_;
    ssl::context& ctx_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    std::shared_ptr<std::string const> doc_root_;
    boost::beast::flat_buffer buffer_;

public:
    explicit
    detect_session(
        tcp::socket socket,
        ssl::context& ctx,
        std::shared_ptr<std::string const> const& doc_root)
        : socket_(std::move(socket))
        , ctx_(ctx)
        , strand_(socket_.get_executor())
        , doc_root_(doc_root)
    {
    }

    // Launch the detector
    void
    run()
    {
        async_detect_ssl(
            socket_,
            buffer_,
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &detect_session::on_detect,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));

    }

    void
    on_detect(boost::system::error_code ec, boost::tribool result)
    {
        if(ec)
            return fail(ec, "detect");

        if(result)
        {
            // Launch SSL session
            std::make_shared<ssl_session>(
                std::move(socket_),
                ctx_,
                std::move(buffer_),
                doc_root_)->run();
            return;
        }

        // Launch plain session
        std::make_shared<plain_session>(
            std::move(socket_),
            std::move(buffer_),
            doc_root_)->run();
    }
};

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    ssl::context& ctx_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::shared_ptr<std::string const> doc_root_;

public:
    listener(
        boost::asio::io_context& ioc,
        ssl::context& ctx,
        tcp::endpoint endpoint,
        std::shared_ptr<std::string const> const& doc_root)
        : ctx_(ctx)
        , strand_(ioc.get_executor())
        , acceptor_(ioc)
        , socket_(ioc)
        , doc_root_(doc_root)
    {
        boost::system::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            boost::asio::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        if(! acceptor_.is_open())
            return;
        do_accept();
    }

    void
    do_accept()
    {
        acceptor_.async_accept(
            socket_,
            std::bind(
                &listener::on_accept,
                shared_from_this(),
                std::placeholders::_1));
    }

    void
    on_accept(boost::system::error_code ec)
    {
        if(ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the detector session and run it
            std::make_shared<detect_session>(
                std::move(socket_),
                ctx_,
                doc_root_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 5)
    {
        std::cerr <<
            "Usage: http-server-flex <address> <port> <doc_root> <threads>\n" <<
            "Example:\n" <<
            "    http-server-flex 0.0.0.0 8080 .\n";
        return EXIT_FAILURE;
    }
    auto const address = boost::asio::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const doc_root = std::make_shared<std::string>(argv[3]);
    auto const threads = std::max<int>(1, std::atoi(argv[4]));

    // The io_context is required for all I/O
    boost::asio::io_context ioc{threads};

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::sslv23};

    // This holds the self-signed certificate used by the server
    load_server_certificate(ctx);

    // Create and launch a listening port
    std::make_shared<listener>(
        ioc,
        ctx,
        tcp::endpoint{address, port},
        doc_root)->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();

    return EXIT_SUCCESS;
}
