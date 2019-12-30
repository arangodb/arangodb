// Copyright (C) 2007-9 Anthony Williams
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE Boost.Threads: thread move test suite

#include <boost/thread/thread_only.hpp>
#include <boost/test/unit_test.hpp>

void do_nothing(boost::thread::id* my_id)
{
    *my_id=boost::this_thread::get_id();
}

BOOST_AUTO_TEST_CASE(test_move_on_construction)
{
    boost::thread::id the_id;
    boost::thread x=boost::thread(do_nothing,&the_id);
    boost::thread::id x_id=x.get_id();
    x.join();
    BOOST_CHECK_EQUAL(the_id,x_id);
}

boost::thread make_thread(boost::thread::id* the_id)
{
    return boost::thread(do_nothing,the_id);
}

BOOST_AUTO_TEST_CASE(test_move_from_function_return)
{
    boost::thread::id the_id;
    boost::thread x=make_thread(&the_id);
    boost::thread::id x_id=x.get_id();
    x.join();
    BOOST_CHECK_EQUAL(the_id,x_id);
}

BOOST_AUTO_TEST_CASE(test_move_assign)
{
    boost::thread::id the_id;
    boost::thread x(do_nothing,&the_id);
    boost::thread y;
    y=boost::move(x);
    boost::thread::id y_id=y.get_id();
    y.join();
    BOOST_CHECK_EQUAL(the_id,y_id);
}


