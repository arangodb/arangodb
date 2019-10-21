//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_YIELD_TO_HPP
#define BOOST_BEAST_TEST_YIELD_TO_HPP

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace boost {
namespace beast {
namespace test {

/** Mix-in to support tests using asio coroutines.

    Derive from this class and use yield_to to launch test
    functions inside coroutines. This is handy for testing
    asynchronous asio code.
*/
class enable_yield_to
{
protected:
    net::io_context ioc_;

private:
    net::executor_work_guard<
        net::io_context::executor_type> work_;
    std::vector<std::thread> threads_;
    std::mutex m_;
    std::condition_variable cv_;
    std::size_t running_ = 0;

public:
    /// The type of yield context passed to functions.
    using yield_context =
        net::yield_context;

    explicit
    enable_yield_to(std::size_t concurrency = 1)
        : work_(ioc_.get_executor())
    {
        threads_.reserve(concurrency);
        while(concurrency--)
            threads_.emplace_back(
                [&]{ ioc_.run(); });
    }

    ~enable_yield_to()
    {
        work_.reset();
        for(auto& t : threads_)
            t.join();
    }

    /// Return the `io_context` associated with the object
    net::io_context&
    get_io_service()
    {
        return ioc_;
    }

    /** Run one or more functions, each in a coroutine.

        This call will block until all coroutines terminate.

        Each functions should have this signature:
        @code
            void f(yield_context);
        @endcode

        @param fn... One or more functions to invoke.
    */
#if BOOST_BEAST_DOXYGEN
    template<class... FN>
    void
    yield_to(FN&&... fn)
#else
    template<class F0, class... FN>
    void
    yield_to(F0&& f0, FN&&... fn);
#endif

private:
    void
    spawn()
    {
    }

    template<class F0, class... FN>
    void
    spawn(F0&& f, FN&&... fn);
};

template<class F0, class... FN>
void
enable_yield_to::
yield_to(F0&& f0, FN&&... fn)
{
    running_ = 1 + sizeof...(FN);
    spawn(f0, fn...);
    std::unique_lock<std::mutex> lock{m_};
    cv_.wait(lock, [&]{ return running_ == 0; });
}

template<class F0, class... FN>
inline
void
enable_yield_to::
spawn(F0&& f, FN&&... fn)
{
    asio::spawn(ioc_,
        [&](yield_context yield)
        {
            f(yield);
            std::lock_guard<std::mutex> lock{m_};
            if(--running_ == 0)
                cv_.notify_all();
        }
        , boost::coroutines::attributes(2 * 1024 * 1024));
    spawn(fn...);
}

} // test
} // beast
} // boost

#endif
