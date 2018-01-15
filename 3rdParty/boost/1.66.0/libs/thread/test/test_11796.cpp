// Copyright (C) 2015 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <iostream>

boost::thread th;
int main()
{

  for (auto ti = 0; ti < 1000; ti++)
  {
    th  = boost::thread([ti]()
      {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        std::cout << ti << std::endl;
      });
  }
  std::string st;

  std::cin >> st;

//  for (int i = 0; i < 10; ++i) {
//    std::cout << "." << i << std::endl;
//    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
//  }
  th.join();
  return 0;
}

