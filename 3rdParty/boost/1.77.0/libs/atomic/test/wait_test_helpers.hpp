//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TEST_WAIT_TEST_HELPERS_HPP_INCLUDED_
#define BOOST_ATOMIC_TEST_WAIT_TEST_HELPERS_HPP_INCLUDED_

#include <boost/memory_order.hpp>
#include <boost/atomic/atomic_flag.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <boost/config.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
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
        notify_one_test< Wrapper, T > test(value1, value2, value3);
        if (test.run())
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
        notify_all_test< Wrapper, T > test(value1, value2);
        if (test.run())
            return;
    }

    BOOST_ERROR("notify_all_test could not complete because blocked thread wake up too soon");
}

//! Invokes all wait/notify tests
template< template< typename > class Wrapper, typename T >
void test_wait_notify_api(T value1, T value2, T value3)
{
    test_wait_value_mismatch< Wrapper >(value1, value2);
    test_notify_one< Wrapper >(value1, value2, value3);
    test_notify_all< Wrapper >(value1, value2);
}


inline void test_flag_wait_notify_api()
{
#ifndef BOOST_ATOMIC_NO_ATOMIC_FLAG_INIT
    boost::atomic_flag f = BOOST_ATOMIC_FLAG_INIT;
#else
    boost::atomic_flag f;
#endif

    bool received_value = f.wait(true);
    BOOST_TEST(!received_value);
    f.notify_one();
    f.notify_all();
}

struct struct_3_bytes
{
    unsigned char data[3u];

    inline bool operator==(struct_3_bytes const& c) const
    {
        return std::memcmp(data, &c.data, sizeof(data)) == 0;
    }
    inline bool operator!=(struct_3_bytes const& c) const
    {
        return std::memcmp(data, &c.data, sizeof(data)) != 0;
    }
};

template< typename Char, typename Traits >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, struct_3_bytes const& val)
{
    strm << "[struct_3_bytes{ " << static_cast< unsigned int >(val.data[0])
        << ", " << static_cast< unsigned int >(val.data[1]) << ", " << static_cast< unsigned int >(val.data[2]) << " }]";
    return strm;
}

struct large_struct
{
    unsigned char data[256u];

    inline bool operator==(large_struct const& c) const
    {
        return std::memcmp(data, &c.data, sizeof(data)) == 0;
    }
    inline bool operator!=(large_struct const& c) const
    {
        return std::memcmp(data, &c.data, sizeof(data)) != 0;
    }
};

template< typename Char, typename Traits >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, large_struct const&)
{
    strm << "[large_struct]";
    return strm;
}

#endif // BOOST_ATOMIC_TEST_WAIT_TEST_HELPERS_HPP_INCLUDED_
