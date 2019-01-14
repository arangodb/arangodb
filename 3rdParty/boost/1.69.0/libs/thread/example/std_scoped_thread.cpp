// (C) Copyright 2009-2012 Anthony Williams
// (C) Copyright 2012 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if __cplusplus < 201103L
int main()
{
  return 0;
}
#else

#define BOOST_THREAD_VERSION 3

#include <iostream>
#include <boost/thread/scoped_thread.hpp>
#include <thread>
#include <cassert>

void do_something(int& i)
{
  ++i;
}
void f(int, int)
{
}

struct func
{
  int& i;

  func(int& i_) :
    i(i_)
  {
  }

  void operator()()
  {
    for (unsigned j = 0; j < 1000000; ++j)
    {
      do_something(i);
    }
  }
};

void do_something_in_current_thread()
{
}

using strict_scoped_thread = boost::strict_scoped_thread<boost::join_if_joinable, std::thread>;
using scoped_thread = boost::scoped_thread<boost::join_if_joinable, std::thread>;

int main()
{
  {
    int some_local_state=0;
    strict_scoped_thread t( (std::thread(func(some_local_state))));

    do_something_in_current_thread();
  }
  {
    int some_local_state=0;
    std::thread t(( func(some_local_state) ));
    strict_scoped_thread g( (boost::move(t)) );

    do_something_in_current_thread();
  }
  {
    int some_local_state=0;
    std::thread t(( func(some_local_state) ));
    strict_scoped_thread g( (std::move(t)) );

    do_something_in_current_thread();
  }
  {
    int some_local_state=1;
    scoped_thread t( (std::thread(func(some_local_state))));

    if (t.joinable()) {
      t.join();
      assert( ! t.joinable() );
    }
    else
      do_something_in_current_thread();
  }
#if 0
  try
  {
    int some_local_state=1;
    std::thread t(( func(some_local_state) ));
    scoped_thread g( (boost::move(t)) );
    if (g.joinable()) {
      // CLANG crash here
      g.detach();
      assert( ! g.joinable() );
    }

    do_something_in_current_thread();
  }
  catch (...) {
    assert( false);
  }
#endif
  {
    scoped_thread g( &f, 1, 2 );
    do_something_in_current_thread();
  }
  return 0;
}

#endif
