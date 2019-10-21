// Copyright (C) 2010 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 2
#define BOOST_THREAD_PROVIDES_INTERRUPTIONS
//#define BOOST_TEST_MODULE Boost.Threads: 2309
//#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/thread.hpp>
#include <boost/detail/lightweight_test.hpp>

  using namespace std;

  boost::mutex mutex_;

  void perform()
  {
     try
     {
        boost::this_thread::sleep(boost::posix_time::seconds(100));
     }
     catch (boost::thread_interrupted& interrupt)
     {
        boost::unique_lock<boost::mutex> lock(mutex_);
        cerr << "Thread " << boost::this_thread::get_id() << " got interrupted" << endl;
        throw(interrupt);
     }
     catch (std::exception& e)
     {
        boost::unique_lock<boost::mutex> lock(mutex_);
        cerr << "Thread " << boost::this_thread::get_id() << " caught std::exception" << e.what() << endl;
     }
     catch (...)
     {
        boost::unique_lock<boost::mutex> lock(mutex_);
        cerr << "Thread " << boost::this_thread::get_id() << " caught something else" << endl;
     }
  }

  void ticket_2309_test()
  {
    try
    {
    boost::thread_group threads;

     for (int i = 0; i < 2; ++i)
     {
        threads.create_thread(perform);
     }

     //boost::this_thread::sleep(1);
     threads.interrupt_all();
     threads.join_all();
    }
    catch (...)
    {
      BOOST_TEST(false && "exception raised");
    }
  }

  int main()
  {

    ticket_2309_test();
  }

