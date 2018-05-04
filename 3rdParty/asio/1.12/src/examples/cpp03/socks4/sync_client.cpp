//
// sync_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <iomanip>
#include <ostream>
#include <string>
#include <asio.hpp>
#include <boost/array.hpp>
#include "socks4.hpp"

using asio::ip::tcp;

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 4)
    {
      std::cout << "Usage: sync_client <socks4server> <socks4port> <user>\n";
      std::cout << "Examples:\n";
      std::cout << "  sync_client 127.0.0.1 1080 chris\n";
      std::cout << "  sync_client localhost socks chris\n";
      return 1;
    }

    asio::io_context io_context;

    // Get a list of endpoints corresponding to the SOCKS 4 server name.
    tcp::resolver resolver(io_context);
    tcp::resolver::results_type endpoints = resolver.resolve(argv[1], argv[2]);

    // Try each endpoint until we successfully establish a connection to the
    // SOCKS 4 server.
    tcp::socket socket(io_context);
    asio::connect(socket, endpoints);

    // Get an endpoint for the Boost website. This will be passed to the SOCKS
    // 4 server. Explicitly specify IPv4 since SOCKS 4 does not support IPv6.
    tcp::endpoint http_endpoint =
      *resolver.resolve(tcp::v4(), "www.boost.org", "http").begin();

    // Send the request to the SOCKS 4 server.
    socks4::request socks_request(
        socks4::request::connect, http_endpoint, argv[3]);
    asio::write(socket, socks_request.buffers());

    // Receive a response from the SOCKS 4 server.
    socks4::reply socks_reply;
    asio::read(socket, socks_reply.buffers());

    // Check whether we successfully negotiated with the SOCKS 4 server.
    if (!socks_reply.success())
    {
      std::cout << "Connection failed.\n";
      std::cout << "status = 0x" << std::hex << socks_reply.status();
      return 1;
    }

    // Form the HTTP request. We specify the "Connection: close" header so that
    // the server will close the socket after transmitting the response. This
    // will allow us to treat all data up until the EOF as the response.
    std::string request =
      "GET / HTTP/1.0\r\n"
      "Host: www.boost.org\r\n"
      "Accept: */*\r\n"
      "Connection: close\r\n\r\n";

    // Send the HTTP request.
    asio::write(socket, asio::buffer(request));

    // Read until EOF, writing data to output as we go.
    boost::array<char, 512> response;
    asio::error_code error;
    while (std::size_t s = socket.read_some(
          asio::buffer(response), error))
      std::cout.write(response.data(), s);
    if (error != asio::error::eof)
      throw asio::system_error(error);
  }
  catch (std::exception& e)
  {
    std::cout << "Exception: " << e.what() << "\n";
  }

  return 0;
}
