// (C) Copyright 2012 Howard Hinnant
// (C) Copyright 2012 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// adapted from the example given by Howard Hinnant in

#include <boost/config.hpp>

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_QUEUE_DEPRECATE_OLD
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif
#include <iostream>
#include <boost/thread/scoped_thread.hpp>
#ifdef XXXX
#include <boost/thread/externally_locked_stream.hpp>
    typedef  boost::externally_locked_stream<std::ostream> the_ostream;
#else
    typedef std::ostream the_ostream;
    typedef std::istream the_istream;
#endif
#include <boost/thread/concurrent_queues/sync_queue.hpp>

void producer(the_ostream & /*mos*/, boost::sync_queue<int> & sbq)
{
  using namespace boost;
  try {
    for(int i=0; ;++i)
    {
      sbq.push(i);
      //sbq << i;
      //mos << "push(" << i << ") "<< sbq.size()<<"\n";
      this_thread::sleep_for(chrono::milliseconds(200));
    }
  }
  catch(sync_queue_is_closed&)
  {
    //mos << "closed !!!\n";
  }
  catch(...)
  {
    //mos << "exception !!!\n";
  }
}

void consumer(
    the_ostream & /*mos*/,
    boost::sync_queue<int> & sbq)
{
  using namespace boost;
  try {
    for(int i=0; ;++i)
    {
      int r;
      sbq.pull(r);
      //sbq >> r;
      //mos << i << " pull(" << r << ") "<< sbq.size()<<"\n";

      this_thread::sleep_for(chrono::milliseconds(250));
    }
  }
  catch(sync_queue_is_closed&)
  {
    //mos << "closed !!!\n";
  }
  catch(...)
  {
    //mos << "exception !!!\n";
  }
}
void consumer2(the_ostream &/*mos*/, boost::sync_queue<int> & sbq)
{
  using namespace boost;
  try {
    for(int i=0; ;++i)
    {
      int r;
      queue_op_status st = sbq.try_pull(r);
      if (queue_op_status::closed == st) break;
      if (queue_op_status::success == st) {
        //mos << i << " pull(" << r << ")\n";
      }
      this_thread::sleep_for(chrono::milliseconds(250));
    }
  }
  catch(...)
  {
    //mos << "exception !!!\n";
  }
}
void consumer3(the_ostream &/*mos*/, boost::sync_queue<int> & sbq)
{
  using namespace boost;
  try {
    for(int i=0; ;++i)
    {
      int r;
      queue_op_status res = sbq.wait_pull(r);
      if (res==queue_op_status::closed) break;
      //mos << i << " wait_pull(" << r << ")\n";
      this_thread::sleep_for(chrono::milliseconds(250));
    }
  }
  catch(...)
  {
    //mos << "exception !!!\n";
  }
}

int main()
{
  using namespace boost;

#ifdef XXXX
  recursive_mutex terminal_mutex;

  externally_locked_stream<std::ostream> mcerr(std::cerr, terminal_mutex);
  externally_locked_stream<std::ostream> mcout(std::cout, terminal_mutex);
  externally_locked_stream<std::istream> mcin(std::cin, terminal_mutex);
#else
  the_ostream &mcerr = std::cout;
  the_ostream &mcout = std::cerr;
  //the_istream &mcin = std::cin;
#endif

  sync_queue<int> sbq;

  {
    mcout << "begin of main" << std::endl;
    scoped_thread<> t11(boost::thread(producer, boost::ref(mcerr), boost::ref(sbq)));
    scoped_thread<> t12(boost::thread(producer, boost::ref(mcerr), boost::ref(sbq)));
    scoped_thread<> t2(boost::thread(consumer, boost::ref(mcout), boost::ref(sbq)));

    this_thread::sleep_for(chrono::seconds(1));

    mcout << "closed()" << std::endl;
    sbq.close();
    mcout << "closed()" << std::endl;

  } // all threads joined here.
  mcout << "end of main" << std::endl;
  return 0;
}

