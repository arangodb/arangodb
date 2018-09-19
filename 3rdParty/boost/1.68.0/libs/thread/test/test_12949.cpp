// Copyright (C) 2017 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4

//#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp>

void f()
{
    boost::this_thread::sleep_for(boost::chrono::milliseconds(10)); // **
}
int main()
{

  return 0;
}

