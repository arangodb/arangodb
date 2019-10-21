//          Copyright Arnaud Kapp, Oliver Kowalke 2016
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <memory>
#include <thread>

#include <boost/asio.hpp>

#include <boost/fiber/all.hpp>
#include "round_robin.hpp"

std::shared_ptr< boost::fibers::unbuffered_channel< int > > c;

void foo() {
    auto io_ptr = std::make_shared< boost::asio::io_service >();
    boost::fibers::use_scheduling_algorithm< boost::fibers::asio::round_robin >( io_ptr);
    boost::fibers::fiber([io_ptr](){
        for ( int i = 0; i < 10; ++i) {
            std::cout << "push " << i << std::endl;
            c->push( i);
        }
		c->close();
		io_ptr->stop();
    }).detach();
    io_ptr->run();
}

void bar() {
    auto io_ptr = std::make_shared< boost::asio::io_service >();
    boost::fibers::use_scheduling_algorithm< boost::fibers::asio::round_robin >( io_ptr);
    boost::fibers::fiber([io_ptr](){
        try {
            for (;;) {
                int i = c->value_pop();
                std::cout << "pop " << i << std::endl;
            }
        } catch ( std::exception const& e) {
            std::cout << "exception: " << e.what() << std::endl;
        }
		io_ptr->stop();
    }).detach();
    io_ptr->run();
}

int main() {
    c = std::make_shared< boost::fibers::unbuffered_channel< int > >();
    std::thread t1( foo);
    std::thread t2( bar);
    t2.join();
    t1.join();
	std::cout << "done." << std::endl;
    return 0;
}
