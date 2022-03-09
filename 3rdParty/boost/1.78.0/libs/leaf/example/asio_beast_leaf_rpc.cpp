// Copyright (c) 2019 Sorin Fetche

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// PLEASE NOTE: This example requires the Boost 1.70 version of Asio and Beast,
// which at the time of this writing is in beta.

// Example of a composed asynchronous operation which uses the LEAF library for
// error handling and reporting.
//
// Examples of running:
// - in one terminal (re)run: ./asio_beast_leaf_rpc_v3 0.0.0.0 8080
// - in another run:
//      curl localhost:8080 -v -d "sum 0 1 2 3"
//   generating errors returned to the client:
//      curl localhost:8080 -v -X DELETE -d ""
//      curl localhost:8080 -v -d "mul 1 2x3"
//      curl localhost:8080 -v -d "div 1 0"
//      curl localhost:8080 -v -d "mod 1"
//
// Runs that showcase the error handling on the server side:
//  - error starting the server:
//      ./asio_beast_leaf_rpc_v3 0.0.0.0 80
//  - error while running the server logic:
//      ./asio_beast_leaf_rpc_v3 0.0.0.0 8080
//      curl localhost:8080 -v -d "error-quit"
//
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/format.hpp>
#include <boost/leaf.hpp>
#include <boost/spirit/include/qi_numeric.hpp>
#include <boost/spirit/include/qi_parse.hpp>
#include <deque>
#include <iostream>
#include <list>
#include <optional>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace leaf = boost::leaf;
namespace net = boost::asio;

namespace {
using error_code = boost::system::error_code;
} // namespace

// The operation being performed when an error occurs.
struct e_last_operation {
    std::string_view value;
};

// The HTTP request type.
using request_t = http::request<http::string_body>;
// The HTTP response type.
using response_t = http::response<http::string_body>;

response_t handle_request(request_t &&request);

// A composed asynchronous operation that implements a basic remote calculator
// over HTTP. It receives from the remote side commands such as:
//      sum 1 2 3
//      div 3 2
//      mod 1 0
// in the body of POST requests and sends back the result.
//
// Besides the calculator related commands, it also offer a special command:
// - `error_quit` that asks the server to simulate a server side error that
//   leads to the connection being dropped.
//
// From the error handling perspective there are three parts of the implementation:
// - the handling of an HTTP request and creating the response to send back
//      (see handle_request)
// - the parsing and execution of the remote command we received as the body of
//   an an HTTP POST request
//      (see execute_command())
// - this composed asynchronous operation which calls them,
//
// This example operation is based on:
// - https://github.com/boostorg/beast/blob/b02f59ff9126c5a17f816852efbbd0ed20305930/example/echo-op/echo_op.cpp
// - part of
// https://github.com/boostorg/beast/blob/b02f59ff9126c5a17f816852efbbd0ed20305930/example/advanced/server/advanced_server.cpp
//
template <class AsyncStream, typename ErrorContext, typename CompletionToken>
auto async_demo_rpc(AsyncStream &stream, ErrorContext &error_context, CompletionToken &&token) ->
    typename net::async_result<typename std::decay<CompletionToken>::type, void(leaf::result<void>)>::return_type {

    static_assert(beast::is_async_stream<AsyncStream>::value, "AsyncStream requirements not met");

    using handler_type =
        typename net::async_completion<CompletionToken, void(leaf::result<void>)>::completion_handler_type;
    using base_type = beast::stable_async_base<handler_type, beast::executor_type<AsyncStream>>;
    struct internal_op : base_type {
        // This object must have a stable address
        struct temporary_data {
            beast::flat_buffer buffer;
            std::optional<http::request_parser<request_t::body_type>> parser;
            std::optional<response_t> response;
        };

        AsyncStream &m_stream;
        ErrorContext &m_error_context;
        temporary_data &m_data;
        bool m_write_and_quit;

        internal_op(AsyncStream &stream, ErrorContext &error_context, handler_type &&handler)
            : base_type{std::move(handler), stream.get_executor()}, m_stream{stream}, m_error_context{error_context},
              m_data{beast::allocate_stable<temporary_data>(*this)}, m_write_and_quit{false} {
            start_read_request();
        }

        void operator()(error_code ec, std::size_t /*bytes_transferred*/ = 0) {
            leaf::result<bool> result_continue_execution;
            {
                auto active_context = activate_context(m_error_context);
                auto load = leaf::on_error(e_last_operation{m_data.response ? "async_demo_rpc::continuation-write"
                                                                           : "async_demo_rpc::continuation-read"});
                if (ec == http::error::end_of_stream) {
                    // The remote side closed the connection.
                    result_continue_execution = false;
                } else if (ec) {
                    result_continue_execution = leaf::new_error(ec);
                } else {
                    result_continue_execution = leaf::exception_to_result([&]() -> leaf::result<bool> {
                        if (!m_data.response) {
                            // Process the request we received.
                            m_data.response = handle_request(std::move(m_data.parser->release()));
                            m_write_and_quit = m_data.response->need_eof();
                            http::async_write(m_stream, *m_data.response, std::move(*this));
                            return true;
                        }

                        // If getting here, we completed a write operation.
                        m_data.response.reset();
                        // And start reading a new message if not quitting (i.e.
                        // the message semantics of the last response we sent
                        // required an end of file)
                        if (!m_write_and_quit) {
                            start_read_request();
                            return true;
                        }

                        // We didn't initiate any new async operation above, so
                        // we will not continue the execution.
                        return false;
                    });
                }
                // The activation object and load_last_operation need to be
                // reset before calling the completion handler This is because,
                // in general, the completion handler may be called directly or
                // posted and if posted, it could execute in another thread.
                // This means that regardless of how the handler gets to be
                // actually called we must ensure that it is not called with the
                // error context active. Note: An error context cannot be
                // activated twice
            }
            if (!result_continue_execution) {
                // We don't continue the execution due to an error, calling the
                // completion handler
                this->complete_now(result_continue_execution.error());
            } else if( !*result_continue_execution ) {
                // We don't continue the execution due to the flag not being
                // set, calling the completion handler
                this->complete_now(leaf::result<void>{});
            }
        }

        void start_read_request() {
            m_data.parser.emplace();
            m_data.parser->body_limit(1024);
            http::async_read(m_stream, m_data.buffer, *m_data.parser, std::move(*this));
        }
    };

    auto initiation = [](auto &&completion_handler, AsyncStream *stream, ErrorContext *error_context) {
        internal_op op{*stream, *error_context, std::forward<decltype(completion_handler)>(completion_handler)};
    };

    // We are in the "initiation" part of the async operation.
    [[maybe_unused]] auto load = leaf::on_error(e_last_operation{"async_demo_rpc::initiation"});
    return net::async_initiate<CompletionToken, void(leaf::result<void>)>(initiation, token, &stream, &error_context);
}

// The location of a int64 parse error. It refers the range of characters from
// which the parsing was done.
struct e_parse_int64_error {
    using location_base = std::pair<std::string_view const, std::string_view::const_iterator>;
    struct location : public location_base {
        using location_base::location_base;

        friend std::ostream &operator<<(std::ostream &os, location const &value) {
            auto const &sv = value.first;
            std::size_t pos = std::distance(sv.begin(), value.second);
            if (pos == 0) {
                os << "->\"" << sv << "\"";
            } else if (pos < sv.size()) {
                os << "\"" << sv.substr(0, pos) << "\"->\"" << sv.substr(pos) << "\"";
            } else {
                os << "\"" << sv << "\"<-";
            }
            return os;
        }
    };

    location value;
};

// Parses an integer from a string_view.
leaf::result<std::int64_t> parse_int64(std::string_view word) {
    auto const begin = word.begin();
    auto const end = word.end();
    std::int64_t value = 0;
    auto i = begin;
    bool result = boost::spirit::qi::parse(i, end, boost::spirit::long_long, value);
    if (!result || i != end) {
        return leaf::new_error(e_parse_int64_error{std::make_pair(word, i)});
    }
    return value;
}

// The command being executed while we get an error. It refers the range of
// characters from which the command was extracted.
struct e_command {
    std::string_view value;
};

// The details about an incorrect number of arguments error Some commands may
// accept a variable number of arguments (e.g. greater than 1 would mean [2,
// SIZE_MAX]).
struct e_unexpected_arg_count {
    struct arg_info {
        std::size_t count;
        std::size_t min;
        std::size_t max;

        friend std::ostream &operator<<(std::ostream &os, arg_info const &value) {
            os << value.count << " (required: ";
            if (value.min == value.max) {
                os << value.min;
            } else if (value.max < SIZE_MAX) {
                os << "[" << value.min << ", " << value.max << "]";
            } else {
                os << "[" << value.min << ", MAX]";
            }
            os << ")";
            return os;
        }
    };

    arg_info value;
};

// The HTTP status that should be returned in case we get into an error.
struct e_http_status {
    http::status value;
};

// Unexpected HTTP method.
struct e_unexpected_http_method {
    http::verb value;
};

// The E-type that describes the `error_quit` command as an error condition.
struct e_error_quit {
    struct none_t {};
    none_t value;
};

// Processes a remote command.
leaf::result<std::string> execute_command(std::string_view line) {
    // Split the command in words.
    std::list<std::string_view> words; // or std::deque<std::string_view> words;

    char const *const ws = "\t \r\n";
    auto skip_ws = [&](std::string_view &line) {
        if (auto pos = line.find_first_not_of(ws); pos != std::string_view::npos) {
            line = line.substr(pos);
        } else {
            line = std::string_view{};
        }
    };

    skip_ws(line);
    while (!line.empty()) {
        std::string_view word;
        if (auto pos = line.find_first_of(ws); pos != std::string_view::npos) {
            word = line.substr(0, pos);
            line = line.substr(pos + 1);
        } else {
            word = line;
            line = std::string_view{};
        }

        if (!word.empty()) {
            words.push_back(word);
        }
        skip_ws(line);
    }

    static char const *const help = "Help:\n"
                                    "    error-quit                  Simulated error to end the session\n"
                                    "    sum <int64>*                Addition\n"
                                    "    sub <int64>+                Substraction\n"
                                    "    mul <int64>*                Multiplication\n"
                                    "    div <int64>+                Division\n"
                                    "    mod <int64> <int64>         Remainder\n"
                                    "    <anything else>             This message";

    if (words.empty()) {
        return std::string(help);
    }

    auto command = words.front();
    words.pop_front();

    auto load_cmd = leaf::on_error(e_command{command}, e_http_status{http::status::bad_request});
    std::string response;

    if (command == "error-quit") {
        return leaf::new_error(e_error_quit{});
    } else if (command == "sum") {
        std::int64_t sum = 0;
        for (auto const &w : words) {
            BOOST_LEAF_AUTO(i, parse_int64(w));
            sum += i;
        }
        response = std::to_string(sum);
    } else if (command == "sub") {
        if (words.size() < 2) {
            return leaf::new_error(e_unexpected_arg_count{words.size(), 2, SIZE_MAX});
        }
        BOOST_LEAF_AUTO(sub, parse_int64(words.front()));
        words.pop_front();
        for (auto const &w : words) {
            BOOST_LEAF_AUTO(i, parse_int64(w));
            sub -= i;
        }
        response = std::to_string(sub);
    } else if (command == "mul") {
        std::int64_t mul = 1;
        for (auto const &w : words) {
            BOOST_LEAF_AUTO(i, parse_int64(w));
            mul *= i;
        }
        response = std::to_string(mul);
    } else if (command == "div") {
        if (words.size() < 2) {
            return leaf::new_error(e_unexpected_arg_count{words.size(), 2, SIZE_MAX});
        }
        BOOST_LEAF_AUTO(div, parse_int64(words.front()));
        words.pop_front();
        for (auto const &w : words) {
            BOOST_LEAF_AUTO(i, parse_int64(w));
            if (i == 0) {
                // In some cases this command execution function might throw,
                // not just return an error.
                throw std::runtime_error{"division by zero"};
            }
            div /= i;
        }
        response = std::to_string(div);
    } else if (command == "mod") {
        if (words.size() != 2) {
            return leaf::new_error(e_unexpected_arg_count{words.size(), 2, 2});
        }
        BOOST_LEAF_AUTO(i1, parse_int64(words.front()));
        words.pop_front();
        BOOST_LEAF_AUTO(i2, parse_int64(words.front()));
        words.pop_front();
        if (i2 == 0) {
            // In some cases this command execution function might throw, not
            // just return an error.
            throw leaf::exception(std::runtime_error{"division by zero"});
        }
        response = std::to_string(i1 % i2);
    } else {
        response = help;
    }

    return response;
}

std::string diagnostic_to_str(leaf::verbose_diagnostic_info const &diag) {
    auto str = boost::str(boost::format("%1%") % diag);
    boost::algorithm::replace_all(str, "\n", "\n    ");
    return "\nDetailed error diagnostic:\n----\n" + str + "\n----";
};

// Handles an HTTP request and returns the response to send back.
response_t handle_request(request_t &&request) {

    auto msg_prefix = [](e_command const *cmd) {
        if (cmd != nullptr) {
            return boost::str(boost::format("Error (%1%):") % cmd->value);
        }
        return std::string("Error:");
    };

    auto make_sr = [](e_http_status const *status, std::string &&response) {
        return std::make_pair(status != nullptr ? status->value : http::status::internal_server_error,
                              std::move(response));
    };

    // In this variant of the RPC example we execute the remote command and
    // handle any errors coming from it in one place (using
    // `leaf::try_handle_all`).
    auto pair_status_response = leaf::try_handle_all(
        [&]() -> leaf::result<std::pair<http::status, std::string>> {
            if (request.method() != http::verb::post) {
                return leaf::new_error(e_unexpected_http_method{http::verb::post},
                                       e_http_status{http::status::bad_request});
            }
            BOOST_LEAF_AUTO(response, execute_command(request.body()));
            return std::make_pair(http::status::ok, std::move(response));
        },
        // For the `error_quit` command and associated error condition we have
        // the error handler itself fail (by throwing). This means that the
        // server will not send any response to the client, it will just
        // shutdown the connection. This implementation showcases two aspects:
        // - that the implementation of error handling can fail, too
        // - how the asynchronous operation calling this error handling function
        //   reacts to this failure.
        [](e_error_quit const &) -> std::pair<http::status, std::string> { throw std::runtime_error("error_quit"); },
        // For the rest of error conditions we just build a message to be sent
        // to the remote client.
        [&](e_parse_int64_error const &e, e_http_status const *status, e_command const *cmd,
            leaf::verbose_diagnostic_info const &diag) {
            return make_sr(status, boost::str(boost::format("%1% int64 parse error: %2%") % msg_prefix(cmd) % e.value) +
                                       diagnostic_to_str(diag));
        },
        [&](e_unexpected_arg_count const &e, e_http_status const *status, e_command const *cmd,
            leaf::verbose_diagnostic_info const &diag) {
            return make_sr(status,
                           boost::str(boost::format("%1% wrong argument count: %2%") % msg_prefix(cmd) % e.value) +
                               diagnostic_to_str(diag));
        },
        [&](e_unexpected_http_method const &e, e_http_status const *status, e_command const *cmd,
            leaf::verbose_diagnostic_info const &diag) {
            return make_sr(status, boost::str(boost::format("%1% unexpected HTTP method. Expected: %2%") %
                                              msg_prefix(cmd) % e.value) +
                                       diagnostic_to_str(diag));
        },
        [&](std::exception const & e, e_http_status const *status, e_command const *cmd,
            leaf::verbose_diagnostic_info const &diag) {
            return make_sr(status, boost::str(boost::format("%1% %2%") % msg_prefix(cmd) % e.what()) +
                                       diagnostic_to_str(diag));
        },
        [&](e_http_status const *status, e_command const *cmd, leaf::verbose_diagnostic_info const &diag) {
            return make_sr(status, boost::str(boost::format("%1% unknown failure") % msg_prefix(cmd)) +
                                       diagnostic_to_str(diag));
        });
    response_t response{pair_status_response.first, request.version()};
    response.set(http::field::server, "Example-with-" BOOST_BEAST_VERSION_STRING);
    response.set(http::field::content_type, "text/plain");
    response.keep_alive(request.keep_alive());
    pair_status_response.second += "\n";
    response.body() = std::move(pair_status_response.second);
    response.prepare_payload();
    return response;
}

int main(int argc, char **argv) {
    auto msg_prefix = [](e_last_operation const *op) {
        if (op != nullptr) {
            return boost::str(boost::format("Error (%1%): ") % op->value);
        }
        return std::string("Error: ");
    };

    // Error handler for internal server internal errors (not communicated to
    // the remote client).
    auto error_handlers = std::make_tuple(
        [&](std::exception_ptr const &ep, e_last_operation const *op) {
            return leaf::try_handle_all(
                [&]() -> leaf::result<int> { std::rethrow_exception(ep); },
                [&](std::exception const & e, leaf::verbose_diagnostic_info const &diag) {
                    std::cerr << msg_prefix(op) << e.what() << " (captured)" << diagnostic_to_str(diag)
                                << std::endl;
                    return -11;
                },
                [&](leaf::verbose_diagnostic_info const &diag) {
                    std::cerr << msg_prefix(op) << "unknown (captured)" << diagnostic_to_str(diag) << std::endl;
                    return -12;
                });
        },
        [&](std::exception const & e, e_last_operation const *op, leaf::verbose_diagnostic_info const &diag) {
            std::cerr << msg_prefix(op) << e.what() << diagnostic_to_str(diag) << std::endl;
            return -21;
        },
        [&](error_code ec, leaf::verbose_diagnostic_info const &diag, e_last_operation const *op) {
            std::cerr << msg_prefix(op) << ec << ":" << ec.message() << diagnostic_to_str(diag) << std::endl;
            return -22;
        },
        [&](leaf::verbose_diagnostic_info const &diag, e_last_operation const *op) {
            std::cerr << msg_prefix(op) << "unknown" << diagnostic_to_str(diag) << std::endl;
            return -23;
        });

    // Top level try block and error handler. It will handle errors from
    // starting the server for example failure to bind to a given port (e.g.
    // ports less than 1024 if not running as root)
    return leaf::try_handle_all(
        [&]() -> leaf::result<int> {
            auto load = leaf::on_error(e_last_operation{"main"});
            if (argc != 3) {
                std::cerr << "Usage: " << argv[0] << " <address> <port>" << std::endl;
                std::cerr << "Example:\n    " << argv[0] << " 0.0.0.0 8080" << std::endl;
                return -1;
            }

            auto const address{net::ip::make_address(argv[1])};
            auto const port{static_cast<std::uint16_t>(std::atoi(argv[2]))};
            net::ip::tcp::endpoint const endpoint{address, port};

            net::io_context io_context;

            // Start the server acceptor and wait for a client.
            net::ip::tcp::acceptor acceptor{io_context, endpoint};

            auto local_endpoint = acceptor.local_endpoint();
            auto address_try_msg = acceptor.local_endpoint().address().to_string();
            if (address_try_msg == "0.0.0.0") {
                address_try_msg = "localhost";
            }
            std::cout << "Server: Started on: " << local_endpoint << std::endl;
            std::cout << "Try in a different terminal:\n"
                      << "    curl " << address_try_msg << ":" << local_endpoint.port() << " -d \"\"\nor\n"
                      << "    curl " << address_try_msg << ":" << local_endpoint.port() << " -d \"sum 1 2 3\""
                      << std::endl;

            auto socket = acceptor.accept();
            std::cout << "Server: Client connected: " << socket.remote_endpoint() << std::endl;

            // The error context for the async operation.
            auto error_context = leaf::make_context(error_handlers);
            int rv = 0;
            async_demo_rpc(socket, error_context, [&](leaf::result<void> result) {
                // Note: In case we wanted to add some additional information to
                // the error associated with the result we would need to
                // activate the error context
                auto active_context = activate_context(error_context);
                if (result) {
                    std::cout << "Server: Client work completed successfully" << std::endl;
                    rv = 0;
                } else {
                    // Handle errors from running the server logic
                    leaf::result<int> result_int{result.error()};
                    rv = error_context.handle_error<int>(result_int.error(), error_handlers);
                }
            });
            io_context.run();

            // Let the remote side know we are shutting down.
            error_code ignored;
            socket.shutdown(net::ip::tcp::socket::shutdown_both, ignored);
            return rv;
        },
        error_handlers);
}
