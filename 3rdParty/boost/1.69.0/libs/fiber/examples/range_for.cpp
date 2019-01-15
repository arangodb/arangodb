
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/fiber/all.hpp>

typedef boost::fibers::unbuffered_channel< unsigned int >	channel_t;

void foo( channel_t & chan) {
	chan.push( 1);
	chan.push( 1);
	chan.push( 2);
	chan.push( 3);
	chan.push( 5);
	chan.push( 8);
	chan.push( 12);
    chan.close();
}

void bar( channel_t & chan) {
    for ( unsigned int value : chan) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
}

int main() {
	try {
        channel_t chan;

        boost::fibers::fiber f1( & foo, std::ref( chan) );
        boost::fibers::fiber f2( & bar, std::ref( chan) );

        f1.join();
        f2.join();

		std::cout << "done." << std::endl;

		return EXIT_SUCCESS;
	} catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
	return EXIT_FAILURE;
}
