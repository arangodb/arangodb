////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <thread>

#include <boost/asio.hpp>
#include "Assertions/Assert.h"

namespace arangodb::tests::mocks {
using boost::asio::ip::tcp;

namespace {

// Simple session class to handle an individual connection
class Session : public std::enable_shared_from_this<Session> {
 public:
  Session(tcp::socket socket) : socket_(std::move(socket)) {}

  void start() { do_read(); }

  void stop() {
    if (socket_.is_open()) {
      boost::system::error_code ec;
      std::ignore = socket_.close(ec);
    }
  }

 private:
  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(data_),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            handle_http_request(length);
          } else {
            // Connection closed or error occurred
            stop();
          }
        });
  }

  void handle_http_request(std::size_t length) {
    // Create a simple HTTP response
    std::string response_body = "Hello from TCP Dummy Server!\n";
    response_body += "Received " + std::to_string(length) + " bytes.\n";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " +
        std::to_string(response_body.length()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        response_body;

    do_write(response);
  }

  void do_write(const std::string& response) {
    auto self(shared_from_this());

    boost::asio::async_write(
        socket_, boost::asio::buffer(response),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            // Close the connection after sending response
            stop();
          } else {
            // Write error occurred
            stop();
          }
        });
  }

  tcp::socket socket_;
  std::array<char, 1024> data_;  // Buffer for received data
};

// Server class to accept new connections
class TcpDummyServer {
 public:
  TcpDummyServer(boost::asio::io_context& io_context, short port)
      : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), running_(true) {
    do_accept();
  }

  ~TcpDummyServer() { stop(); }

  void stop() {
    running_ = false;
    if (acceptor_.is_open()) {
      boost::system::error_code ec;
      std::ignore = acceptor_.close(ec);
    }
  }

 private:
  void do_accept() {
    if (!running_) {
      return;
    }

    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
          if (!ec && running_) {
            std::make_shared<Session>(std::move(socket))->start();
          }
          // Continue accepting new connections only if still running
          if (running_) {
            do_accept();
          }
        });
  }

  tcp::acceptor acceptor_;
  std::atomic<bool> running_;
};
}  // namespace

class MockTcpServer final {
 public:
  MockTcpServer(int port) : tcpDummyServer(io_context, port) {}

  ~MockTcpServer() { TRI_ASSERT(io_context.stopped()); }

  void start() {
    serverThread = std::jthread([this]() { io_context.run(); });
  }

  void stop() {
    tcpDummyServer.stop();
    io_context.stop();
    if (serverThread.joinable()) {
      serverThread.join();
    }
  }

 private:
  boost::asio::io_context io_context;
  std::jthread serverThread;
  TcpDummyServer tcpDummyServer;
};

}  // namespace arangodb::tests::mocks
