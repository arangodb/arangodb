// Copyright (C) 2007 Anthony Williams
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE Boost.Threads: thread::get_id test suite

#include <boost/thread/thread_only.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>

void do_nothing()
{}

BOOST_AUTO_TEST_CASE(test_thread_id_for_default_constructed_thread_is_default_constructed_id)
{
    boost::thread t;
    BOOST_CHECK(t.get_id()==boost::thread::id());
}

BOOST_AUTO_TEST_CASE(test_thread_id_for_running_thread_is_not_default_constructed_id)
{
    boost::thread t(do_nothing);
    BOOST_CHECK(t.get_id()!=boost::thread::id());
    t.join();
}

BOOST_AUTO_TEST_CASE(test_different_threads_have_different_ids)
{
    boost::thread t(do_nothing);
    boost::thread t2(do_nothing);
    BOOST_CHECK(t.get_id()!=t2.get_id());
    t.join();
    t2.join();
}

BOOST_AUTO_TEST_CASE(test_thread_ids_have_a_total_order)
{
    boost::thread t(do_nothing);
    boost::thread t2(do_nothing);
    boost::thread t3(do_nothing);
    BOOST_CHECK(t.get_id()!=t2.get_id());
    BOOST_CHECK(t.get_id()!=t3.get_id());
    BOOST_CHECK(t2.get_id()!=t3.get_id());

    BOOST_CHECK((t.get_id()<t2.get_id()) != (t2.get_id()<t.get_id()));
    BOOST_CHECK((t.get_id()<t3.get_id()) != (t3.get_id()<t.get_id()));
    BOOST_CHECK((t2.get_id()<t3.get_id()) != (t3.get_id()<t2.get_id()));

    BOOST_CHECK((t.get_id()>t2.get_id()) != (t2.get_id()>t.get_id()));
    BOOST_CHECK((t.get_id()>t3.get_id()) != (t3.get_id()>t.get_id()));
    BOOST_CHECK((t2.get_id()>t3.get_id()) != (t3.get_id()>t2.get_id()));

    BOOST_CHECK((t.get_id()<t2.get_id()) == (t2.get_id()>t.get_id()));
    BOOST_CHECK((t2.get_id()<t.get_id()) == (t.get_id()>t2.get_id()));
    BOOST_CHECK((t.get_id()<t3.get_id()) == (t3.get_id()>t.get_id()));
    BOOST_CHECK((t3.get_id()<t.get_id()) == (t.get_id()>t3.get_id()));
    BOOST_CHECK((t2.get_id()<t3.get_id()) == (t3.get_id()>t2.get_id()));
    BOOST_CHECK((t3.get_id()<t2.get_id()) == (t2.get_id()>t3.get_id()));

    BOOST_CHECK((t.get_id()<t2.get_id()) == (t2.get_id()>=t.get_id()));
    BOOST_CHECK((t2.get_id()<t.get_id()) == (t.get_id()>=t2.get_id()));
    BOOST_CHECK((t.get_id()<t3.get_id()) == (t3.get_id()>=t.get_id()));
    BOOST_CHECK((t3.get_id()<t.get_id()) == (t.get_id()>=t3.get_id()));
    BOOST_CHECK((t2.get_id()<t3.get_id()) == (t3.get_id()>=t2.get_id()));
    BOOST_CHECK((t3.get_id()<t2.get_id()) == (t2.get_id()>=t3.get_id()));

    BOOST_CHECK((t.get_id()<=t2.get_id()) == (t2.get_id()>t.get_id()));
    BOOST_CHECK((t2.get_id()<=t.get_id()) == (t.get_id()>t2.get_id()));
    BOOST_CHECK((t.get_id()<=t3.get_id()) == (t3.get_id()>t.get_id()));
    BOOST_CHECK((t3.get_id()<=t.get_id()) == (t.get_id()>t3.get_id()));
    BOOST_CHECK((t2.get_id()<=t3.get_id()) == (t3.get_id()>t2.get_id()));
    BOOST_CHECK((t3.get_id()<=t2.get_id()) == (t2.get_id()>t3.get_id()));

    if((t.get_id()<t2.get_id()) && (t2.get_id()<t3.get_id()))
    {
        BOOST_CHECK(t.get_id()<t3.get_id());
    }
    else if((t.get_id()<t3.get_id()) && (t3.get_id()<t2.get_id()))
    {
        BOOST_CHECK(t.get_id()<t2.get_id());
    }
    else if((t2.get_id()<t3.get_id()) && (t3.get_id()<t.get_id()))
    {
        BOOST_CHECK(t2.get_id()<t.get_id());
    }
    else if((t2.get_id()<t.get_id()) && (t.get_id()<t3.get_id()))
    {
        BOOST_CHECK(t2.get_id()<t3.get_id());
    }
    else if((t3.get_id()<t.get_id()) && (t.get_id()<t2.get_id()))
    {
        BOOST_CHECK(t3.get_id()<t2.get_id());
    }
    else if((t3.get_id()<t2.get_id()) && (t2.get_id()<t.get_id()))
    {
        BOOST_CHECK(t3.get_id()<t.get_id());
    }
    else
    {
        BOOST_CHECK(false);
    }

    boost::thread::id default_id;

    BOOST_CHECK(default_id < t.get_id());
    BOOST_CHECK(default_id < t2.get_id());
    BOOST_CHECK(default_id < t3.get_id());

    BOOST_CHECK(default_id <= t.get_id());
    BOOST_CHECK(default_id <= t2.get_id());
    BOOST_CHECK(default_id <= t3.get_id());

    BOOST_CHECK(!(default_id > t.get_id()));
    BOOST_CHECK(!(default_id > t2.get_id()));
    BOOST_CHECK(!(default_id > t3.get_id()));

    BOOST_CHECK(!(default_id >= t.get_id()));
    BOOST_CHECK(!(default_id >= t2.get_id()));
    BOOST_CHECK(!(default_id >= t3.get_id()));

    t.join();
    t2.join();
    t3.join();
}

void get_thread_id(boost::thread::id* id)
{
    *id=boost::this_thread::get_id();
}

BOOST_AUTO_TEST_CASE(test_thread_id_of_running_thread_returned_by_this_thread_get_id)
{
    boost::thread::id id;
    boost::thread t(boost::bind(get_thread_id,&id));
    boost::thread::id t_id=t.get_id();
    t.join();
    BOOST_CHECK(id==t_id);
}
