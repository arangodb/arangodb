// Copyright (C) 2014 Ian Forbed
// Copyright (C) 2014 Vicente J. Botet Escriba
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

#include <exception>

#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/thread/concurrent_queues/sync_priority_queue.hpp>

#include <boost/core/lightweight_test.hpp>

#ifdef BOOST_MSVC
#pragma warning(disable: 4127) // conditional expression is constant
#endif

typedef boost::concurrent::sync_priority_queue<int> sync_pq;

int call_pull(sync_pq* q, boost::barrier* go)
{
    go->wait();
    return q->pull();

}

void call_push(sync_pq* q, boost::barrier* go, int val)
{
    go->wait();
    q->push(val);
}

void test_pull(const int n)
{
    sync_pq pq;
    BOOST_TEST(pq.empty());
    for(int i  = 0; i < n; i++)
    {
        pq.push(i);
    }
    BOOST_TEST(!pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(n));
    pq.close();
    BOOST_TEST(pq.closed());
    boost::barrier b(n);
    boost::thread_group tg;
    for(int i = 0; i < n; i++)
    {
        tg.create_thread(boost::bind(call_pull, &pq, &b));
    }
    tg.join_all();
    BOOST_TEST(pq.empty());
}

void test_push(const int n)
{
    sync_pq pq;
    BOOST_TEST(pq.empty());

    boost::barrier b(n);
    boost::thread_group tg;
    for(int i  = 0; i < n; i++)
    {
        tg.create_thread(boost::bind(call_push, &pq, &b, i));
    }
    tg.join_all();
    BOOST_TEST(!pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(n));
}

void test_both(const int n)
{
    sync_pq pq;
    BOOST_TEST(pq.empty());

    boost::barrier b(2*n);
    boost::thread_group tg;
    for(int i  = 0; i < n; i++)
    {
        tg.create_thread(boost::bind(call_pull, &pq, &b));
        tg.create_thread(boost::bind(call_push, &pq, &b, i));
    }
    tg.join_all();
    BOOST_TEST(pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(0));
}

void push_range(sync_pq* q, const int begin, const int end)
{
    for(int i = begin; i < end; i++)
        q->push(i);
}

void atomic_pull(sync_pq* q, boost::atomic<int>* sum)
{
    while(1)
    {
        try{
            const int val = q->pull();
            sum->fetch_add(val);
        }
        catch(std::exception&  ){
            break;
        }
    }
}

/**
 * This test computes the sum of the first N integers upto $limit using
 * $n threads for the push operation and $n threads for the pull and count
 * operation. The push operation push a range of numbers on the queue while
 * the pull operation pull from the queue and increments an atomic int.
 * At the end of execution the value of atomic<int> $sum should be the same
 * as n*(n+1)/2 as this is the closed form solution to this problem.
 */
void compute_sum(const int n)
{
    const int limit = 1000;
    sync_pq pq;
    BOOST_TEST(pq.empty());
    boost::atomic<int> sum(0);
    boost::thread_group tg1;
    boost::thread_group tg2;
    for(int i = 0; i < n; i++)
    {
        tg1.create_thread(boost::bind(push_range, &pq, i*(limit/n)+1, (i+1)*(limit/n)+1));
        tg2.create_thread(boost::bind(atomic_pull, &pq, &sum));
    }
    tg1.join_all();
    pq.close();  //Wait until all enqueuing is done before closing.
    BOOST_TEST(pq.closed());
    tg2.join_all();
    BOOST_TEST(pq.empty());
    BOOST_TEST_EQ(sum.load(), limit*(limit+1)/2);
}

void move_between_queues(sync_pq* q1, sync_pq* q2)
{
    while(1){
        try{
            const int val = q1->pull();
            q2->push(val);
        }
        catch(std::exception& ){
            break;
        }
    }
}

/**
 * This test computes the sum of the first N integers upto $limit by moving
 * numbers between 2 sync_priority_queues. A range of numbers are pushed onto
 * one queue by $n threads while $n threads pull from this queue and push onto
 * another sync_pq. At the end the main thread ensures the the values in the
 * second queue are in proper order and then sums all the values from this
 * queue. The sum should match n*(n+1)/2, the closed form solution to this
 * problem.
 */
void sum_with_moving(const int n)
{
    const int limit = 1000;
    sync_pq pq1;
    sync_pq pq2;
    BOOST_TEST(pq1.empty());
    BOOST_TEST(pq2.empty());
    boost::thread_group tg1;
    boost::thread_group tg2;
    for(int i = 0; i < n; i++)
    {
        tg1.create_thread(boost::bind(push_range, &pq1, i*(limit/n)+1, (i+1)*(limit/n)+1));
        tg2.create_thread(boost::bind(move_between_queues, &pq1, &pq2));
    }
    tg1.join_all();
    pq1.close();  //Wait until all enqueuing is done before closing.
    BOOST_TEST(pq1.closed());
    tg2.join_all();
    BOOST_TEST(pq1.empty());
    BOOST_TEST(!pq2.empty());
    int sum = 0;
    for(int i = 1000; i > 0; i--){
        const int val = pq2.pull();
        BOOST_TEST_EQ(i,val);
        sum += val;
    }
    BOOST_TEST(pq2.empty());
    BOOST_TEST_EQ(sum, limit*(limit+1)/2);
}

int main()
{
    for(int i = 1; i <= 64; i *= 2)
    {
        test_pull(i);
        test_push(i);
        test_both(i);
    }
    //These numbers must divide 1000
    compute_sum(1);
    compute_sum(4);
    compute_sum(10);
    compute_sum(25);
    compute_sum(50);
    sum_with_moving(1);
    sum_with_moving(4);
    sum_with_moving(10);
    sum_with_moving(25);
    sum_with_moving(50);
    return boost::report_errors();
}
