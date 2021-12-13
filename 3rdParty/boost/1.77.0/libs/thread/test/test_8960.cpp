// Copyright (C) 2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/thread/thread.hpp>
#include <iostream>

#include <iostream>

#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/chrono.hpp>
//#include <boost/bind/bind.hpp>
#include <boost/detail/lightweight_test.hpp>


void do_thread()
{

  try
  {
    boost::condition_variable c1;
    boost::mutex m1;
    boost::unique_lock<boost::mutex> l1(m1);

    c1.wait_for(l1, boost::chrono::seconds(1));
  }
  catch (std::runtime_error& ex)
  {
    std::cout << "EXCEPTION ! " << ex.what() << std::endl;
    BOOST_TEST(false);

  }
  catch (...)
  {
    std::cout << "EXCEPTION ! " << std::endl;
    BOOST_TEST(false);
  }

}

int main()
{

  boost::thread th1(&do_thread);
  th1.join();

  //std::string s1;
  //std::cin >> s1;

  return boost::report_errors();
}
