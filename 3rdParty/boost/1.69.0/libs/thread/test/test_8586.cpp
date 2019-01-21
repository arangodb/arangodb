// Copyright (C) 2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/thread/thread.hpp>
#include <iostream>

void hello_world()
{
    std::cout << "Hello from thread!" << std::endl;
}

int main()
{
    boost::thread my_thread(&hello_world);
    my_thread.join();
}
