
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/fiber/all.hpp>

int main() {
    using channel_t = boost::fibers::buffered_channel< std::string >;
	try {
        channel_t chan1{ 2 }, chan2{ 2 };

        boost::fibers::fiber fping([&chan1,&chan2]{
                    chan1.push( "ping");
                    std::cout << chan2.value_pop() << "\n";
                    chan1.push( "ping");
                    std::cout << chan2.value_pop() << "\n";
                    chan1.push( "ping");
                    std::cout << chan2.value_pop() << "\n";
                });
        boost::fibers::fiber fpong([&chan1,&chan2]{
                    std::cout << chan1.value_pop() << "\n";
                    chan2.push( "pong");
                    std::cout << chan1.value_pop() << "\n";
                    chan2.push( "pong");
                    std::cout << chan1.value_pop() << "\n";
                    chan2.push( "pong");
                });

        fping.join();
        fpong.join();

		std::cout << "done." << std::endl;

		return EXIT_SUCCESS;
	}
	catch ( std::exception const& e)
	{ std::cerr << "exception: " << e.what() << std::endl; }
	catch (...)
	{ std::cerr << "unhandled exception" << std::endl; }
	return EXIT_FAILURE;
}
