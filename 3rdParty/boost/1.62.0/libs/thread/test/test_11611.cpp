// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//#define BOOST_THREAD_VERSION 4

#include <iostream>
//#include <thread>

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#include <boost/thread/executors/loop_executor.hpp>
#include <boost/thread/executors/serial_executor_cont.hpp>
#include <boost/thread/executors/serial_executor.hpp>
#include <boost/thread/thread.hpp>

using namespace std;

int main()
{
   boost::loop_executor ex;

   //thread t([&ex]()
   boost::thread t([&ex]()
   {
      ex.loop();
   });

   {
     //boost::serial_executor_cont serial(ex);
     boost::serial_executor serial(ex);

      for (size_t i = 0; i < 100000; i++)
         serial.submit([i] {
             //std::cout << i << ".";
         });

      serial.close();
   }

   ex.close();

   t.join();
   std::cout << "end" << std::endl;
   return 0;
}
