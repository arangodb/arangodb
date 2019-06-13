//  (C) Copyright 2009 Anthony Williams
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/thread/thread_only.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/future.hpp>
#include <utility>
#include <memory>
#include <string>

#define BOOST_TEST_MODULE Boost.Threads: thread exit test suite

#include <boost/test/unit_test.hpp>

boost::thread::id exit_func_thread_id;

void exit_func()
{
    exit_func_thread_id=boost::this_thread::get_id();
}

void tf1()
{
    boost::this_thread::at_thread_exit(exit_func);
    BOOST_CHECK(exit_func_thread_id!=boost::this_thread::get_id());
}

BOOST_AUTO_TEST_CASE(test_thread_exit_func_runs_when_thread_exits)
{
    exit_func_thread_id=boost::thread::id();
    boost::thread t(&tf1);
    boost::thread::id const t_id=t.get_id();
    t.join();
    BOOST_CHECK(exit_func_thread_id==t_id);
}

struct fo
{
    void operator()()
    {
        exit_func_thread_id=boost::this_thread::get_id();
    }
};

void tf2()
{
    boost::this_thread::at_thread_exit(fo());
    BOOST_CHECK(exit_func_thread_id!=boost::this_thread::get_id());
}


BOOST_AUTO_TEST_CASE(test_can_use_function_object_for_exit_func)
{
    exit_func_thread_id=boost::thread::id();
    boost::thread t(tf2);
    boost::thread::id const t_id=t.get_id();
    t.join();
    BOOST_CHECK(exit_func_thread_id==t_id);
}
