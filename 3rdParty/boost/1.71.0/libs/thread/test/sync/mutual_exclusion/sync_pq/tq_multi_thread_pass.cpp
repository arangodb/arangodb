// Copyright (C) 2019 Austin Beer
// Copyright (C) 2019 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/concurrent_queues/sync_timed_queue.hpp>

#include <boost/core/lightweight_test.hpp>
#include "../../../timming.hpp"

using namespace boost::chrono;

typedef boost::concurrent::sync_timed_queue<int> sync_tq;

const int cnt = 5;

void call_push(sync_tq* q, const steady_clock::time_point start)
{
    // push elements onto the queue every 500 milliseconds but with a decreasing delay each time
    for (int i = 0; i < cnt; ++i)
    {
        boost::this_thread::sleep_until(start + milliseconds(i * 500));
        const steady_clock::time_point expected = start + milliseconds(i * 500) + seconds(cnt - i);
        q->push(i, expected);
    }
}

void call_pull(sync_tq* q, const steady_clock::time_point start)
{
    // pull elements off of the queue (earliest element first)
    for (int i = cnt - 1; i >= 0; --i)
    {
        int j;
        q->pull(j);
        BOOST_TEST_EQ(i, j);
        const steady_clock::time_point expected = start + milliseconds(i * 500) + seconds(cnt - i);
        BOOST_TEST_GE(steady_clock::now(), expected - milliseconds(BOOST_THREAD_TEST_TIME_MS));
        BOOST_TEST_LE(steady_clock::now(), expected + milliseconds(BOOST_THREAD_TEST_TIME_MS));
    }
}

void call_pull_until(sync_tq* q, const steady_clock::time_point start)
{
    // pull elements off of the queue (earliest element first)
    for (int i = cnt - 1; i >= 0; --i)
    {
        int j;
        q->pull_until(steady_clock::now() + hours(1), j);
        BOOST_TEST_EQ(i, j);
        const steady_clock::time_point expected = start + milliseconds(i * 500) + seconds(cnt - i);
        BOOST_TEST_GE(steady_clock::now(), expected - milliseconds(BOOST_THREAD_TEST_TIME_MS));
        BOOST_TEST_LE(steady_clock::now(), expected + milliseconds(BOOST_THREAD_TEST_TIME_MS));
    }
}

void call_pull_for(sync_tq* q, const steady_clock::time_point start)
{
    // pull elements off of the queue (earliest element first)
    for (int i = cnt - 1; i >= 0; --i)
    {
        int j;
        q->pull_for(hours(1), j);
        BOOST_TEST_EQ(i, j);
        const steady_clock::time_point expected = start + milliseconds(i * 500) + seconds(cnt - i);
        BOOST_TEST_GE(steady_clock::now(), expected - milliseconds(BOOST_THREAD_TEST_TIME_MS));
        BOOST_TEST_LE(steady_clock::now(), expected + milliseconds(BOOST_THREAD_TEST_TIME_MS));
    }
}

void test_push_while_pull()
{
    sync_tq tq;
    BOOST_TEST(tq.empty());
    boost::thread_group tg;
    const steady_clock::time_point start = steady_clock::now();
    tg.create_thread(boost::bind(call_push, &tq, start));
    tg.create_thread(boost::bind(call_pull, &tq, start));
    tg.join_all();
    BOOST_TEST(tq.empty());
}

void test_push_while_pull_until()
{
    sync_tq tq;
    BOOST_TEST(tq.empty());
    boost::thread_group tg;
    const steady_clock::time_point start = steady_clock::now();
    tg.create_thread(boost::bind(call_push, &tq, start));
    tg.create_thread(boost::bind(call_pull_until, &tq, start));
    tg.join_all();
    BOOST_TEST(tq.empty());
}

void test_push_while_pull_for()
{
    sync_tq tq;
    BOOST_TEST(tq.empty());
    boost::thread_group tg;
    const steady_clock::time_point start = steady_clock::now();
    tg.create_thread(boost::bind(call_push, &tq, start));
    tg.create_thread(boost::bind(call_pull_for, &tq, start));
    tg.join_all();
    BOOST_TEST(tq.empty());
}

int main()
{
    test_push_while_pull();
    test_push_while_pull_until();
    test_push_while_pull_for();
    return boost::report_errors();
}
