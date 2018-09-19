//          Copyright Nat Goodspeed + Oliver Kowalke 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <boost/assert.hpp>

#include <boost/fiber/all.hpp>

#include "thread_barrier.hpp"

static std::size_t fiber_count{ 0 };
static std::mutex mtx_count{};
static boost::fibers::condition_variable_any cnd_count{};
typedef std::unique_lock< std::mutex > lock_type;

/*****************************************************************************
*   example fiber function
*****************************************************************************/
//[fiber_fn_ws
void whatevah( char me) {
    try {
        std::thread::id my_thread = std::this_thread::get_id(); /*< get ID of initial thread >*/
        {
            std::ostringstream buffer;
            buffer << "fiber " << me << " started on thread " << my_thread << '\n';
            std::cout << buffer.str() << std::flush;
        }
        for ( unsigned i = 0; i < 10; ++i) { /*< loop ten times >*/
            boost::this_fiber::yield(); /*< yield to other fibers >*/
            std::thread::id new_thread = std::this_thread::get_id(); /*< get ID of current thread >*/
            if ( new_thread != my_thread) { /*< test if fiber was migrated to another thread >*/
                my_thread = new_thread;
                std::ostringstream buffer;
                buffer << "fiber " << me << " switched to thread " << my_thread << '\n';
                std::cout << buffer.str() << std::flush;
            }
        }
    } catch ( ... ) {
    }
    lock_type lk( mtx_count);
    if ( 0 == --fiber_count) { /*< Decrement fiber counter for each completed fiber. >*/
        lk.unlock();
        cnd_count.notify_all(); /*< Notify all fibers waiting on `cnd_count`. >*/
    }
}
//]

/*****************************************************************************
*   example thread function
*****************************************************************************/
//[thread_fn_ws
void thread( thread_barrier * b) {
    std::ostringstream buffer;
    buffer << "thread started " << std::this_thread::get_id() << std::endl;
    std::cout << buffer.str() << std::flush;
    boost::fibers::use_scheduling_algorithm< boost::fibers::algo::work_stealing >( 4); /*<
        Install the scheduling algorithm `boost::fibers::algo::work_stealing` in order to
        join the work sharing.
    >*/
    b->wait(); /*< sync with other threads: allow them to start processing >*/
    lock_type lk( mtx_count);
    cnd_count.wait( lk, [](){ return 0 == fiber_count; } ); /*<
        Suspend main fiber and resume worker fibers in the meanwhile.
        Main fiber gets resumed (e.g returns from `condition_variable_any::wait()`)
        if all worker fibers are complete.
    >*/
    BOOST_ASSERT( 0 == fiber_count);
}
//]

/*****************************************************************************
*   main()
*****************************************************************************/
int main( int argc, char *argv[]) {
    std::cout << "main thread started " << std::this_thread::get_id() << std::endl;
//[main_ws
    boost::fibers::use_scheduling_algorithm< boost::fibers::algo::work_stealing >( 4); /*<
        Install the scheduling algorithm `boost::fibers::algo::work_stealing` in the main thread
        too, so each new fiber gets launched into the shared pool.
    >*/

    for ( char c : std::string("abcdefghijklmnopqrstuvwxyz")) { /*<
        Launch a number of worker fibers; each worker fiber picks up a character
        that is passed as parameter to fiber-function `whatevah`.
        Each worker fiber gets detached.
    >*/
        boost::fibers::fiber([c](){ whatevah( c); }).detach();
        ++fiber_count; /*< Increment fiber counter for each new fiber. >*/
    }
    thread_barrier b( 4);
    std::thread threads[] = { /*<
        Launch a couple of threads that join the work sharing.
    >*/
        std::thread( thread, & b),
        std::thread( thread, & b),
        std::thread( thread, & b)
    };
    b.wait(); /*< sync with other threads: allow them to start processing >*/
    {
        lock_type/*< `lock_type` is typedef'ed as __unique_lock__< [@http://en.cppreference.com/w/cpp/thread/mutex `std::mutex`] > >*/ lk( mtx_count);
        cnd_count.wait( lk, [](){ return 0 == fiber_count; } ); /*<
            Suspend main fiber and resume worker fibers in the meanwhile.
            Main fiber gets resumed (e.g returns from `condition_variable_any::wait()`)
            if all worker fibers are complete.
        >*/
    } /*<
        Releasing lock of mtx_count is required before joining the threads, otherwise
        the other threads would be blocked inside condition_variable::wait() and
        would never return (deadlock).
    >*/
    BOOST_ASSERT( 0 == fiber_count);
    for ( std::thread & t : threads) { /*< wait for threads to terminate >*/
        t.join();
    }
//]
    std::cout << "done." << std::endl;
    return EXIT_SUCCESS;
}
