// Copyright (C) 2009 Anthony Williams
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_USES_MOVE

#define BOOST_TEST_MODULE Boost.Threads: thread return local test suite

#include <boost/thread/thread_only.hpp>
#include <boost/test/unit_test.hpp>

void do_nothing(boost::thread::id* my_id)
{
    *my_id=boost::this_thread::get_id();
}

boost::thread make_thread_return_local(boost::thread::id* the_id)
{
    boost::thread t(do_nothing,the_id);
    return boost::move(t);
}

BOOST_AUTO_TEST_CASE(test_move_from_function_return_local)
{
    boost::thread::id the_id;
    boost::thread x=make_thread_return_local(&the_id);
    boost::thread::id x_id=x.get_id();
    x.join();
    BOOST_CHECK_EQUAL(the_id,x_id);
}
