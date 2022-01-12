//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TEST_IPC_WAIT_TEST_HELPERS_HPP_INCLUDED_
#define BOOST_ATOMIC_TEST_IPC_WAIT_TEST_HELPERS_HPP_INCLUDED_

#include <boost/memory_order.hpp>
#include <boost/atomic/ipc_atomic_flag.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <boost/config.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/atomic/capabilities.hpp>
#include <boost/atomic/ipc_atomic_flag.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>
#include "atomic_wrapper.hpp"
#include "lightweight_test_stream.hpp"
#include "test_clock.hpp"

//! Since some of the tests below are allowed to fail, we retry up to this many times to pass the test
BOOST_CONSTEXPR_OR_CONST unsigned int test_retry_count = 5u;

//! The test verifies that the wait operation returns immediately if the passed value does not match the atomic value
template< template< typename > class Wrapper, typename T >
inline void test_wait_value_mismatch(T value1, T value2)
{
    Wrapper< T > m_wrapper(value1);

    T received_value = m_wrapper.a.wait(value2);
    BOOST_TEST(received_value == value1);
}

/*!
 * The test verifies that notify_one releases one blocked thread and that the released thread receives the modified atomic value.
 *
 * Technically, this test is allowed to fail since wait() is allowed to return spuriously. However, normally this should not happen.
 */
template< template< typename > class Wrapper, typename T >
class notify_one_test
{
private:
    struct thread_state
    {
        T m_received_value;
        test_clock::time_point m_wakeup_time;

        explicit thread_state(T value) : m_received_value(value)
        {
        }
    };

private:
    Wrapper< T > m_wrapper;

    char m_padding[1024];

    T m_value1, m_value2, m_value3;

    boost::barrier m_barrier;

    thread_state m_thread1_state;
    thread_state m_thread2_state;

public:
    explicit notify_one_test(T value1, T value2, T value3) :
        m_wrapper(value1),
        m_value1(value1),
        m_value2(value2),
        m_value3(value3),
        m_barrier(3),
        m_thread1_state(value1),
        m_thread2_state(value1)
    {
    }

    bool run()
    {
        boost::thread thread1(&notify_one_test::thread_func, this, &m_thread1_state);
        boost::thread thread2(&notify_one_test::thread_func, this, &m_thread2_state);

        m_barrier.wait();

        test_clock::time_point start_time = test_clock::now();

        boost::this_thread::sleep_for(chrono::milliseconds(200));

        m_wrapper.a.store(m_value2, boost::memory_order_release);
        m_wrapper.a.notify_one();

        boost::this_thread::sleep_for(chrono::milliseconds(200));

        m_wrapper.a.store(m_value3, boost::memory_order_release);
        m_wrapper.a.notify_one();

        if (!thread1.try_join_for(chrono::seconds(3)))
        {
            BOOST_ERROR("Thread 1 failed to join");
            std::abort();
        }
        if (!thread2.try_join_for(chrono::seconds(3)))
        {
            BOOST_ERROR("Thread 2 failed to join");
            std::abort();
        }

        thread_state* first_state = &m_thread1_state;
        thread_state* second_state = &m_thread2_state;
        if (second_state->m_wakeup_time < first_state->m_wakeup_time)
            std::swap(first_state, second_state);

        if (m_wrapper.a.has_native_wait_notify())
        {
            if ((first_state->m_wakeup_time - start_time) < chrono::milliseconds(200))
            {
                std::cout << "notify_one_test: first thread woke up too soon: " << chrono::duration_cast< chrono::milliseconds >(first_state->m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            if ((first_state->m_wakeup_time - start_time) >= chrono::milliseconds(400))
            {
                std::cout << "notify_one_test: first thread woke up too late: " << chrono::duration_cast< chrono::milliseconds >(first_state->m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            if ((second_state->m_wakeup_time - start_time) < chrono::milliseconds(400))
            {
                std::cout << "notify_one_test: second thread woke up too soon: " << chrono::duration_cast< chrono::milliseconds >(second_state->m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            BOOST_TEST_EQ(first_state->m_received_value, m_value2);
            BOOST_TEST_EQ(second_state->m_received_value, m_value3);
        }
        else
        {
            // With the emulated wait/notify the threads are most likely to return prior to notify
            BOOST_TEST(first_state->m_received_value == m_value2 || first_state->m_received_value == m_value3);
            BOOST_TEST(second_state->m_received_value == m_value2 || second_state->m_received_value == m_value3);
        }

        return true;
    }

private:
    void thread_func(thread_state* state)
    {
        m_barrier.wait();

        state->m_received_value = m_wrapper.a.wait(m_value1);
        state->m_wakeup_time = test_clock::now();
    }
};

template< template< typename > class Wrapper, typename T >
inline void test_notify_one(T value1, T value2, T value3)
{
    for (unsigned int i = 0u; i < test_retry_count; ++i)
    {
        // Avoid creating IPC atomics on the stack as this breaks on Darwin
        boost::scoped_ptr< notify_one_test< Wrapper, T > > test(new notify_one_test< Wrapper, T >(value1, value2, value3));
        if (test->run())
            return;
    }

    BOOST_ERROR("notify_one_test could not complete because blocked thread wake up too soon");
}

/*!
 * The test verifies that notify_all releases all blocked threads and that the released threads receive the modified atomic value.
 *
 * Technically, this test is allowed to fail since wait() is allowed to return spuriously. However, normally this should not happen.
 */
template< template< typename > class Wrapper, typename T >
class notify_all_test
{
private:
    struct thread_state
    {
        T m_received_value;
        test_clock::time_point m_wakeup_time;

        explicit thread_state(T value) : m_received_value(value)
        {
        }
    };

private:
    Wrapper< T > m_wrapper;

    char m_padding[1024];

    T m_value1, m_value2;

    boost::barrier m_barrier;

    thread_state m_thread1_state;
    thread_state m_thread2_state;

public:
    explicit notify_all_test(T value1, T value2) :
        m_wrapper(value1),
        m_value1(value1),
        m_value2(value2),
        m_barrier(3),
        m_thread1_state(value1),
        m_thread2_state(value1)
    {
    }

    bool run()
    {
        boost::thread thread1(&notify_all_test::thread_func, this, &m_thread1_state);
        boost::thread thread2(&notify_all_test::thread_func, this, &m_thread2_state);

        m_barrier.wait();

        test_clock::time_point start_time = test_clock::now();

        boost::this_thread::sleep_for(chrono::milliseconds(200));

        m_wrapper.a.store(m_value2, boost::memory_order_release);
        m_wrapper.a.notify_all();

        if (!thread1.try_join_for(chrono::seconds(3)))
        {
            BOOST_ERROR("Thread 1 failed to join");
            std::abort();
        }
        if (!thread2.try_join_for(chrono::seconds(3)))
        {
            BOOST_ERROR("Thread 2 failed to join");
            std::abort();
        }

        if (m_wrapper.a.has_native_wait_notify())
        {
            if ((m_thread1_state.m_wakeup_time - start_time) < chrono::milliseconds(200))
            {
                std::cout << "notify_all_test: first thread woke up too soon: " << chrono::duration_cast< chrono::milliseconds >(m_thread1_state.m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            if ((m_thread2_state.m_wakeup_time - start_time) < chrono::milliseconds(200))
            {
                std::cout << "notify_all_test: second thread woke up too soon: " << chrono::duration_cast< chrono::milliseconds >(m_thread2_state.m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }
        }

        BOOST_TEST_EQ(m_thread1_state.m_received_value, m_value2);
        BOOST_TEST_EQ(m_thread2_state.m_received_value, m_value2);

        return true;
    }

private:
    void thread_func(thread_state* state)
    {
        m_barrier.wait();

        state->m_received_value = m_wrapper.a.wait(m_value1);
        state->m_wakeup_time = test_clock::now();
    }
};

template< template< typename > class Wrapper, typename T >
inline void test_notify_all(T value1, T value2)
{
    for (unsigned int i = 0u; i < test_retry_count; ++i)
    {
        // Avoid creating IPC atomics on the stack as this breaks on Darwin
        boost::scoped_ptr< notify_all_test< Wrapper, T > > test(new notify_all_test< Wrapper, T >(value1, value2));
        if (test->run())
            return;
    }

    BOOST_ERROR("notify_all_test could not complete because blocked thread wake up too soon");
}

//! Invokes all wait/notify tests
template< template< typename > class Wrapper, typename T >
void test_wait_notify_api_impl(T value1, T value2, T value3, boost::true_type)
{
    test_wait_value_mismatch< Wrapper >(value1, value2);
    test_notify_one< Wrapper >(value1, value2, value3);
    test_notify_all< Wrapper >(value1, value2);
}

template< template< typename > class Wrapper, typename T >
inline void test_wait_notify_api_impl(T, T, T, boost::false_type)
{
}

//! Invokes all wait/notify tests, if the atomic type is lock-free
template< template< typename > class Wrapper, typename T >
inline void test_wait_notify_api(T value1, T value2, T value3)
{
    test_wait_notify_api_impl< Wrapper >(value1, value2, value3, boost::integral_constant< bool, Wrapper< T >::atomic_type::is_always_lock_free >());
}

//! Invokes all wait/notify tests, if the atomic type is lock-free
template< template< typename > class Wrapper, typename T >
inline void test_wait_notify_api(T value1, T value2, T value3, int has_native_wait_notify_macro)
{
    BOOST_TEST_EQ(Wrapper< T >::atomic_type::always_has_native_wait_notify, (has_native_wait_notify_macro == 2));
    test_wait_notify_api< Wrapper >(value1, value2, value3);
}


inline void test_flag_wait_notify_api()
{
#if BOOST_ATOMIC_FLAG_LOCK_FREE == 2
#ifndef BOOST_ATOMIC_NO_ATOMIC_FLAG_INIT
    boost::ipc_atomic_flag f = BOOST_ATOMIC_FLAG_INIT;
#else
    boost::ipc_atomic_flag f;
#endif

    bool received_value = f.wait(true);
    BOOST_TEST(!received_value);
    f.notify_one();
    f.notify_all();
#endif // BOOST_ATOMIC_FLAG_LOCK_FREE == 2
}

#endif // BOOST_ATOMIC_TEST_IPC_WAIT_TEST_HELPERS_HPP_INCLUDED_
