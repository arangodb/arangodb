// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4

// BoostFutureTest.cpp : Defines the entry point for the console application.
//

#include <iostream>

// boost version 1.60.0
// has the following set.
#define BOOST_THREAD_VERSION 4
// #define BOOST_THREAD_PROVIDES_EXECUTORS

#include <boost/thread/future.hpp>


int main()
{
    int value = 0;
    int tmpValue = 0;
    boost::promise<void> promise1;
    boost::promise<void> promise2;

    auto future1 = promise1.get_future();

    auto waitFuture = future1.then([&tmpValue, &promise2](boost::future<void> future){
        assert(future.is_ready());  // this works correctly and is ready.

        auto fut = boost::async(boost::launch::async, [&promise2, &tmpValue](){
          boost::this_thread::sleep_for(boost::chrono::seconds(1));
            tmpValue = 1;
            promise2.set_value();
            std::cout << "Step 2 "<< std::endl; // should print 1 but prints 0
        });
        std::cout << "Step 1 "<< std::endl; // should print 1 but prints 0

        return promise2.get_future();
        //return ;
    }).then([&value, &tmpValue](boost::future<boost::future<void>> future){
    //}).then([&value, &tmpValue](boost::future<void> future){
      // error: no matching function for call to ‘boost::future<boost::future<void> >::then(main()::<lambda(boost::future<void>)>)’
      // as expected

        assert(future.is_ready());  // this doesn't work correctly and is not ready.


        value = tmpValue;
    });


    promise1.set_value();
    waitFuture.wait();

    std::cout << "value = " << value << std::endl; // should print 1 but prints 0
    return 0;
}

