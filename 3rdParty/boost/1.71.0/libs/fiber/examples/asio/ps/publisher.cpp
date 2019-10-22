//
// blocking_tcp_echo_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

enum {
    max_length = 1024
};

int main( int argc, char* argv[]) {
    try {
        if ( 3 != argc) {
            std::cerr << "Usage: publisher <host> <queue>\n";
            return EXIT_FAILURE;
        }
        boost::asio::io_service io_service;
        tcp::resolver resolver( io_service);
        tcp::resolver::query query( tcp::v4(), argv[1], "9997");
        tcp::resolver::iterator iterator = resolver.resolve(query);
        tcp::socket s( io_service);
        boost::asio::connect( s, iterator);
        char msg[max_length];
        std::string queue( argv[2]);
        std::memset( msg, '\0', max_length);
        std::memcpy( msg, queue.c_str(), queue.size() );
        boost::asio::write( s, boost::asio::buffer( msg, max_length) );
        for (;;) {
            std::cout << "publish: ";
            char request[max_length];
            std::cin.getline( request, max_length);
            boost::asio::write( s, boost::asio::buffer( request, max_length) );
        }
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return EXIT_FAILURE;
}
